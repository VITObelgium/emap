#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
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
    ModelResult(EmissionIdentifier id_, gdx::DenseRaster<double> ras) noexcept
    : id(id_)
    , emissions(std::move(ras))
    {
    }

    EmissionIdentifier id;
    gdx::DenseRaster<double> emissions;
};

void spread_emissions(const EmissionInventory& inventory, const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    ModelProgress progress(inventory.size(), progressCb);
    progress.set_payload(ModelRunProgressInfo());

    // TODO: smarter splitting to avoid huge memory consumption
    tbb::concurrent_vector<ModelResult> result;
    result.reserve(inventory.size());

    tbb::parallel_for_each(inventory, [&](const auto& entry) {
        if (auto spatialPatternPath = cfg.spatial_pattern_path(entry.id()); fs::is_regular_file(spatialPatternPath)) {
            auto spatialPattern = gdx::read_dense_raster<double>(spatialPatternPath);
            spatialPattern *= entry.diffuse_emissions();

            //result.emplace_back(entry.id(), std::move(spatialPattern));
            const auto outputFilename = fs::u8path(fmt::format("{}_{}_{}.tif", entry.id().pollutant.code(), entry.id().sector.name(), entry.id().country.code()));
            gdx::write_raster(spatialPattern, cfg.output_path() / "emissions" / std::to_string(static_cast<int>(cfg.year())) / outputFilename);
        }

        progress.tick();
    });

    /*struct RasterMessage
    {
        double diffuseEmissions = 0.0;
        EmissionIdentifier emissionId;
        std::shared_ptr<gdx::DenseRaster<double>> raster;
    };

    const int maxRasters = cfg.max_concurrency().value_or(tbb::this_task_arena::max_concurrency() + 4);
    auto iter            = inventory.begin();
    tbb::filter<void, RasterMessage> source(tbb::filter_mode::serial_in_order, [&](tbb::flow_control& fc) {
        if (iter == inventory.end() || progress.cancel_requested()) {
            fc.stop();
            return RasterMessage();
        }

        RasterMessage msg;
        msg.diffuseEmissions = iter->diffuse_emissions();
        msg.emissionId       = iter->id();
        if (auto spatialPatternPath = cfg.spatial_pattern_path(iter->id()); fs::is_regular_file(spatialPatternPath)) {
            msg.raster = std::make_shared<gdx::DenseRaster<double>>(gdx::read_dense_raster<double>(spatialPatternPath));
        }

        ++iter;
        return msg;
    });

    tbb::filter<RasterMessage, RasterMessage> transform(tbb::filter_mode::parallel, [&](RasterMessage msg) {
        if (!progress.cancel_requested() && msg.raster) {
            *msg.raster *= msg.diffuseEmissions;
        }

        return msg;
    });

    tbb::filter<RasterMessage, void> writer(tbb::filter_mode::parallel, [&](RasterMessage msg) {
        if (!progress.cancel_requested() && msg.raster) {
            const auto outputFilename = fs::u8path(fmt::format("{}_{}_{}.tif", msg.emissionId.pollutant.code(), msg.emissionId.sector.name(), msg.emissionId.country.code()));
            gdx::write_raster(*msg.raster, cfg.output_path() / "emissions" / std::to_string(static_cast<int>(cfg.year())) / outputFilename);
        }

        progress.tick();
    });

    tbb::filter<void, void> chain = source & transform & writer;
    tbb::parallel_pipeline(maxRasters, chain);*/
}

void run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    const auto pointSourcesFlanders = parse_point_sources_flanders(throw_if_not_exists(cfg.point_source_emissions_path()));

    const auto nfrTotalEmissions   = parse_emissions(EmissionSector::Type::Nfr, throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Nfr)));
    const auto gnfrTotalEmissions  = parse_emissions(EmissionSector::Type::Gnfr, throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Gnfr)));
    const auto scalingsDiffuse     = parse_scaling_factors(throw_if_not_exists(cfg.diffuse_scalings_path()));
    const auto scalingsPointSource = parse_scaling_factors(throw_if_not_exists(cfg.point_source_scalings_path()));

    Log::debug("Create country coverages");
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
