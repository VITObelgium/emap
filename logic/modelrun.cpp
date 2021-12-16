#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/outputwriters.h"
#include "emap/preprocessing.h"
#include "emap/scalingfactors.h"
#include "emissioninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/log.h"

#include "gdx/denserasterio.h"

#include <numeric>
#include <oneapi/tbb/concurrent_vector.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/parallel_pipeline.h>
#include <oneapi/tbb/task_arena.h>

namespace emap {

using namespace inf;

static fs::path throw_if_not_exists(fs::path&& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return std::move(path);
}

void run_model(const fs::path& runConfigPath, const ModelProgress::Callback& progressCb)
{
    Log::debug("Process configuration: {}", runConfigPath);

    // Check if there is a preprocessing step defined
    run_preprocessing(runConfigPath, [&progressCb](const PreprocessingProgress::Status& status) {
        if (progressCb) {
            return progressCb(ModelProgress::Status(status.current(), status.total(), status.payload()));
        }

        return ProgressStatusResult::Continue;
    });

    if (auto runConfig = parse_run_configuration_file(runConfigPath); runConfig.has_value()) {
        return run_model(*runConfig, progressCb);
    } else {
        Log::debug("No model run configured");
    }
}

struct ModelResult
{
    ModelResult() noexcept = default;
    ModelResult(EmissionIdentifier id_, std::vector<BrnOutputEntry> ras) noexcept
    : id(id_)
    , emissions(std::move(ras))
    {
    }

    EmissionIdentifier id;
    std::vector<BrnOutputEntry> emissions;
};

void spread_emissions(const EmissionInventory& inventory, const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    ModelProgress progress(enum_count<Pollutant::Id>() * enum_count<NfrSector>() * enum_count<Country::Id>(), progressCb);
    progress.set_payload(ModelRunProgressInfo());

    // TODO: smarter splitting to avoid huge memory consumption
    tbb::concurrent_vector<ModelResult> result;
    result.reserve(inventory.size());

    /*tbb::parallel_for_each(inventory, [&](const auto& entry) {
        if (auto spatialPatternPath = cfg.spatial_pattern_path(cfg.year(), entry.id()); fs::is_regular_file(spatialPatternPath)) {
            auto spatialPattern = gdx::read_dense_raster<double>(spatialPatternPath);
            spatialPattern *= entry.diffuse_emissions();

            gdx::write_raster(spatialPattern, cfg.emission_output_raster_path(cfg.year(), entry.id()));
        }

        progress.tick();
    });*/

    struct EmissionInputMsg
    {
        EmissionInventoryEntry emissions;
        fs::path spatialPattern;
    };

    struct EmissionSpreadResultMsg
    {
        std::shared_ptr<gdx::DenseRaster<double>> raster;
    };

    struct EmissionResultMsg
    {
        EmissionInventoryEntry emissions;
        std::shared_ptr<std::vector<BrnOutputEntry>> brnEmissions;
    };

    const int maxRasters = cfg.max_concurrency().value_or(tbb::this_task_arena::max_concurrency() + 4);

    // Run a pipeline for each pollutant
    for (auto pollutant : inf::enum_entries<Pollutant::Id>()) {
        for (auto sector : inf::enum_entries<NfrSector>()) {
            for (auto country : inf::enum_entries<Country::Id>()) {
                progress.tick();

                EmissionIdentifier emissionId(Country(country), EmissionSector(sector), pollutant);

                const auto emissions = inventory.emissions_with_id(emissionId);
                if (emissions.empty()) {
                    Log::debug("No emissions available for pollutant {} in sector: {} in {}", Pollutant(pollutant), EmissionSector(sector), Country(country));
                    continue;
                } else if (emissions.size() > 1) {
                    Log::debug("Multiple emissions available for pollutant {} in sector: {} in {}", Pollutant(pollutant), EmissionSector(sector), Country(country));
                    continue;
                }

                gdx::DenseRaster<double> resultRaster;
                if (auto spatialPatternPath = cfg.spatial_pattern_path(cfg.year(), emissionId); fs::is_regular_file(spatialPatternPath)) {
                    auto raster = gdx::read_dense_raster<double>(spatialPatternPath);
                    raster *= emissions.front().diffuse_emissions();

                    const auto gridMeta = grid_data(cfg.grid_definition()).meta;
                    auto warped         = gdx::warp_raster(raster, gridMeta.projected_epsg().value(), gdal::ResampleAlgorithm::Sum);
                    if (resultRaster.empty()) {
                        resultRaster = std::move(warped);
                    } else {
                        resultRaster.add_or_assign(warped);
                    }
                }

                /*auto iter = emissions.begin();
                tbb::filter<void, EmissionInputMsg> source(tbb::filter_mode::serial_in_order, [&](tbb::flow_control& fc) {
                    if (iter == emissions.end() || progress.cancel_requested()) {
                        fc.stop();
                        return EmissionInputMsg();
                    }

                    EmissionInputMsg msg;
                    msg.emissions = **iter;
                    if (auto spatialPatternPath = cfg.spatial_pattern_path(cfg.year(), (*iter)->id()); fs::is_regular_file(spatialPatternPath)) {
                        msg.spatialPattern = spatialPatternPath;
                    }

                    ++iter;
                    return msg;
                });

                tbb::filter<EmissionInputMsg, EmissionSpreadResultMsg> spread(tbb::filter_mode::parallel, [&](EmissionInputMsg msg) {
                    EmissionSpreadResultMsg outMsg;
                    if (!progress.cancel_requested() && !msg.spatialPattern.empty()) {
                        auto raster = gdx::read_dense_raster<double>(msg.spatialPattern);
                        raster *= msg.emissions.diffuse_emissions();

                        const auto gridMeta = grid_data(cfg.grid_definition()).meta;
                        outMsg.raster       = std::make_shared<gdx::DenseRaster<double>>(gdx::warp_raster(raster, gridMeta.projected_epsg().value(), gdal::ResampleAlgorithm::Sum));
                        Log::info("Warped: {}", msg.spatialPattern);
                    }

                    return outMsg;
                });

                tbb::filter<EmissionSpreadResultMsg, void> add(tbb::filter_mode::serial_in_order, [&](EmissionSpreadResultMsg msg) {
                    if (!progress.cancel_requested() && msg.raster) {
                        if (resultRaster.empty()) {
                            resultRaster = std::move(*msg.raster);
                        } else {
                            resultRaster += *msg.raster;
                        }
                    }

                    progress.tick();
                });

                tbb::filter<void, void> chain = source & spread & add;
                tbb::parallel_pipeline(maxRasters, chain);*/

                const auto& meta = resultRaster.metadata();

                std::vector<BrnOutputEntry> brnEntries;
                brnEntries.reserve(resultRaster.size());
                for (auto cell : gdx::RasterCells(resultRaster)) {
                    if (resultRaster.is_nodata(cell) || resultRaster[cell] == 0.0) {
                        continue;
                    }

                    const auto cellCenter = meta.convert_cell_centre_to_xy(cell);

                    BrnOutputEntry entry;
                    entry.x    = truncate<int64_t>(cellCenter.x);
                    entry.y    = truncate<int64_t>(cellCenter.y);
                    entry.q    = resultRaster[cell];
                    entry.comp = pollutant;
                    brnEntries.push_back(entry);
                }

                if (!brnEntries.empty()) {
                    write_brn_output(brnEntries, cfg.emission_brn_output_path(cfg.year(), pollutant, EmissionSector(sector)));
                }
            }
        }
    }
}

void run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    //const auto pointSourcesFlanders = parse_point_sources_flanders(throw_if_not_exists(cfg.point_source_emissions_path()));
    SingleEmissions pointSourcesFlanders;

    chrono::DurationRecorder duration;
    const auto nfrTotalEmissions = parse_emissions(EmissionSector::Type::Nfr, throw_if_not_exists(cfg.total_emissions_path_nfr()));
    Log::info("Parse nfr emissions took: {}", duration.elapsed_time_string());
    duration.reset();
    const auto gnfrTotalEmissions = parse_emissions(EmissionSector::Type::Gnfr, throw_if_not_exists(cfg.total_emissions_path_gnfr()));
    Log::info("Parse gnfr emissions took: {}", duration.elapsed_time_string());

    const ScalingFactors scalingsDiffuse;
    const ScalingFactors scalingsPointSource;

    //const auto scalingsDiffuse     = parse_scaling_factors(throw_if_not_exists(cfg.diffuse_scalings_path()));
    //const auto scalingsPointSource = parse_scaling_factors(throw_if_not_exists(cfg.point_source_scalings_path()));

    auto gridData = grid_data(cfg.grid_definition());

    Log::debug("Generate emission inventory");
    chrono::DurationRecorder dur;
    const auto inventory = create_emission_inventory(nfrTotalEmissions, pointSourcesFlanders, scalingsDiffuse, scalingsPointSource);
    Log::debug("Generate emission inventory took {}", dur.elapsed_time_string());

    Log::debug("Spread emissions");
    dur.reset();
    spread_emissions(inventory, cfg, progressCb);
    Log::debug("Spread emissions took {}", dur.elapsed_time_string());
}
}
