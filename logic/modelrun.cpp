#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/outputbuilderfactory.h"
#include "emap/scalingfactors.h"
#include "emissioninventory.h"
#include "outputwriters.h"
#include "runsummary.h"
#include "spatialpatterninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/log.h"

#include "gdx/denserasterio.h"

#include <numeric>
#include <oneapi/tbb/concurrent_vector.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/parallel_pipeline.h>
#include <oneapi/tbb/task_arena.h>

#include <fmt/printf.h>

namespace emap {

using namespace inf;

static fs::path throw_if_not_exists(fs::path&& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return std::move(path);
}

void run_model(const fs::path& runConfigPath, inf::Log::Level logLevel, const ModelProgress::Callback& progressCb)
{
    if (auto runConfig = parse_run_configuration_file(runConfigPath); runConfig.has_value()) {
        std::unique_ptr<inf::LogRegistration> logReg;
        inf::Log::add_file_sink(runConfig->output_path() / "emap.log");

        logReg = std::make_unique<inf::LogRegistration>("e-map");
        inf::Log::set_level(logLevel);

        return run_model(*runConfig, progressCb);
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

static gdx::DenseRaster<double> apply_spatial_pattern_raster(const fs::path& rasterPath, const EmissionInventoryEntry& emission, const GridData& grid, std::span<const CellCoverageInfo> cellCoverages)
{
    // The returned raster is normalized
    auto raster = extract_country_from_raster(rasterPath, cellCoverages);
    raster *= emission.diffuse_emissions();

    const auto gridMeta = grid.meta;
    return gdx::warp_raster(raster, gridMeta.projected_epsg().value(), gdal::ResampleAlgorithm::Sum);
}

static gdx::DenseRaster<double> apply_spatial_pattern_ceip(const fs::path& ceipPath, const EmissionInventoryEntry& emission, const GridData& grid, const RunConfiguration& cfg)
{
    // The returned raster is not normalized, it contains only values for the requested country
    auto raster = parse_spatial_pattern_ceip(ceipPath, emission.id(), cfg);
    normalize_raster(raster);
    raster *= emission.diffuse_emissions();

    const auto gridMeta = grid.meta;
    return gdx::warp_raster(raster, gridMeta.projected_epsg().value(), gdal::ResampleAlgorithm::Sum);
}

static gdx::DenseRaster<double> apply_spatial_pattern_table(const fs::path& tablePath, const EmissionInventoryEntry& emission, const RunConfiguration& cfg)
{
    if (emission.id().country == country::BEF) {
        return parse_spatial_pattern_flanders(tablePath, emission.id().sector, cfg);
    }

    throw RuntimeError("Spatial pattern tables are only implemented for Flanders, not {}", emission.id().country);
}

static gdx::DenseRaster<double> apply_uniform_spread(const EmissionInventoryEntry& emission, const GridData& grid, std::span<const CellCoverageInfo> cellCoverages)
{
    return spread_values_uniformly_over_cells(emission.scaled_diffuse_emissions(), grid.type, cellCoverages);
}

static gdx::DenseRaster<double> apply_spatial_pattern(const SpatialPatternSource& spatialPattern, const EmissionInventoryEntry& emission, const GridData& grid, std::span<const CellCoverageInfo> cellCoverages, const RunConfiguration& cfg)
{
    switch (spatialPattern.type) {
    case SpatialPatternSource::Type::SpatialPatternCAMS:
        return apply_spatial_pattern_raster(spatialPattern.path, emission, grid, cellCoverages);
    case SpatialPatternSource::Type::SpatialPatternCEIP:
        throw apply_spatial_pattern_ceip(spatialPattern.path, emission, grid, cfg);
    case SpatialPatternSource::Type::SpatialPatternTable:
        return apply_spatial_pattern_table(spatialPattern.path, emission, cfg);
    case SpatialPatternSource::Type::UnfiformSpread:
        return apply_uniform_spread(emission, grid, cellCoverages);
        break;
    }

    throw RuntimeError("Invalid spatial pattern type");
}

void spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, IOutputBuilder& output, const ModelProgress::Callback& progressCb)
{
    const auto camsGrid = grid_data(GridDefinition::CAMS);

    Log::debug("Create country coverages");

    ModelProgressInfo progressInfo;
    ProgressTracker progress(known_countries_in_extent(cfg.countries(), camsGrid.meta, cfg.countries_vector_path(), cfg.country_field_id()), progressCb);

    // Precompute the cell coverages per country as it can be expensive
    chrono::DurationRecorder dur;
    const auto countryCoverages = create_country_coverages(camsGrid.meta, cfg.countries_vector_path(), cfg.country_field_id(), cfg.countries(), [&](const GridProcessingProgress::ProgressTracker::Status& status) {
        progressInfo.info = fmt::format("Calculate region cells: {}", status.payload().full_name());
        progress.set_payload(progressInfo);
        progress.tick();
        return ProgressStatusResult::Continue;
    });
    Log::debug("Create country coverages took {}", dur.elapsed_time_string());

    //ModelProgress progress(cfg.pollutants().pollutant_count() * cfg.sectors().nfr_sector_count() * cfg.countries().country_count(), progressCb);
    //progress.set_payload(ModelRunProgressInfo());

    // TODO: smarter splitting to avoid huge memory consumption
    tbb::concurrent_vector<ModelResult> result;
    result.reserve(emissionInv.size());

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
    for (auto country : cfg.countries().countries()) {
        const auto* countryCellCoverages = find_in_container(countryCoverages, [&](const CountryCellCoverage& cov) {
            return cov.first == country;
        });

        if (countryCellCoverages == nullptr) {
            Log::warn("NO cell coverage info for country: {}", country.iso_code());
            continue;
        }

        for (auto pollutant : cfg.pollutants().list()) {
            for (auto& sector : cfg.sectors().nfr_sectors()) {
                progress.tick();

                EmissionIdentifier emissionId(Country(country), EmissionSector(sector), pollutant);

                const auto emissions = emissionInv.emissions_with_id(emissionId);
                if (emissions.empty()) {
                    Log::debug("No emissions available for pollutant {} in sector: {} in {}", Pollutant(pollutant), EmissionSector(sector), Country(country));
                    continue;
                } else if (emissions.size() > 1) {
                    Log::debug("Multiple emissions available for pollutant {} in sector: {} in {}", Pollutant(pollutant), EmissionSector(sector), Country(country));
                    continue;
                }

                const auto resultRaster = apply_spatial_pattern(spatialPatternInv.get_spatial_pattern(emissionId), emissions.front(), grid_data(cfg.grid_definition()), countryCellCoverages->second, cfg);
                const auto& meta        = resultRaster.metadata();

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

static SingleEmissions read_point_sources(const RunConfiguration& cfg, const Country& country, RunSummary& runSummary)
{
    SingleEmissions result;

    if (country == country::BEF) {
        for (const auto& pollutant : cfg.pollutants().list()) {
            const auto path = cfg.point_source_emissions_path(country, pollutant);
            if (fs::is_regular_file(path)) {
                merge_emissions(result, parse_point_sources(path, cfg));
                runSummary.add_point_source(path);
            }
        }
    }

    try {
        const auto pm10     = cfg.pollutants().try_pollutant_from_string("PM10");
        const auto pm2_5    = cfg.pollutants().try_pollutant_from_string("PM2.5");
        const auto pmCoarse = cfg.pollutants().try_pollutant_from_string("PMcoarse");

        if (pm10.has_value() &&
            pm2_5.has_value() &&
            pmCoarse.has_value() &&
            fs::is_regular_file(cfg.point_source_emissions_path(country, *pm10)) &&
            fs::is_regular_file(cfg.point_source_emissions_path(country, *pm2_5)) &&
            !fs::is_regular_file(cfg.point_source_emissions_path(country, *pmCoarse))) {
            // No PMcoarse data provided, create it from pm10 and pm2.5 data
            const auto pm10Path  = cfg.point_source_emissions_path(country, *pm10);
            const auto pm2_5Path = cfg.point_source_emissions_path(country, *pm2_5);

            auto pm10Emissions  = parse_point_sources(pm10Path, cfg);
            auto pm2_5Emissions = parse_point_sources(pm2_5Path, cfg);

            std::sort(pm2_5Emissions.begin(), pm2_5Emissions.end(), [](const EmissionEntry& lhs, const EmissionEntry& rhs) {
                return lhs.source_id() < rhs.source_id();
            });

            SingleEmissions pmCoarseEmissions;

            for (auto& pm10Entry : pm10Emissions) {
                auto iter = std::lower_bound(pm2_5Emissions.begin(), pm2_5Emissions.end(), pm10Entry.source_id(), [](const EmissionEntry& entry, std::string_view srcId) {
                    return entry.source_id() < srcId;
                });

                if (iter != pm2_5Emissions.end() && iter->source_id() == pm10Entry.source_id()) {
                    auto pm10Val  = pm10Entry.value().amount();
                    auto pm2_5Val = iter->value().amount();

                    if (pm10Val.has_value() >= pm2_5Val.has_value()) {
                        if (pm10Val >= pm2_5Val) {
                            auto pmCoarseVal = EmissionValue(*pm10Val - *pm2_5Val);
                            result.update_or_add_emission(EmissionEntry(EmissionIdentifier(country, pm10Entry.id().sector, *pmCoarse), pmCoarseVal));
                        } else {
                            throw RuntimeError("Invalid PM data for sector {} (PM10: {}, PM2.5 {})", pm10Entry.id().sector, *pm10, *pm2_5);
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        Log::debug("Failed to create PMcoarse point sources: {}", e.what());
    }

    return result;
}

static SingleEmissions read_nfr_emissions(const RunConfiguration& cfg, RunSummary& runSummary)
{
    chrono::DurationRecorder duration;
    auto nfrTotalEmissions = parse_emissions(EmissionSector::Type::Nfr, throw_if_not_exists(cfg.total_emissions_path_nfr()), cfg);
    runSummary.add_totals_source(cfg.total_emissions_path_nfr());
    Log::debug("Parse nfr emissions took: {}", duration.elapsed_time_string());
    duration.reset();

    static const std::array<const Country*, 3> belgianRegions = {
        &country::BEB,
        &country::BEF,
        &country::BEW,
    };

    for (auto* region : belgianRegions) {
        const auto& path = cfg.total_emissions_path_nfr_belgium(*region);
        merge_emissions(nfrTotalEmissions, parse_emissions_belgium(path, cfg.year(), cfg));
        runSummary.add_totals_source(path);
    }

    return nfrTotalEmissions;
}

static SingleEmissions read_gnfr_emissions(const RunConfiguration& cfg, RunSummary& runSummary)
{
    chrono::DurationRecorder duration;
    const auto gnfrTotalEmissions = parse_emissions(EmissionSector::Type::Gnfr, throw_if_not_exists(cfg.total_emissions_path_gnfr()), cfg);
    runSummary.add_totals_source(cfg.total_emissions_path_gnfr());
    Log::debug("Parse gnfr emissions took: {}", duration.elapsed_time_string());
    return gnfrTotalEmissions;
}

void run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    RunSummary summary;

    auto outputBuilder = make_output_builder(cfg);

    SpatialPatternInventory spatPatInv(cfg.sectors(), cfg.pollutants());
    spatPatInv.scan_dir(cfg.reporting_year(), cfg.year(), cfg.spatial_pattern_path());

    // Create the list of spatial patterns that will be used in the model
    for (auto& nfr : cfg.sectors().nfr_sectors()) {
        for (auto pol : cfg.pollutants().list()) {
            // Take any european country, except for a Belgian region
            summary.add_spatial_pattern_source(spatPatInv.get_spatial_pattern(cfg.countries().non_belgian_country(), pol, EmissionSector(nfr)));
            summary.add_country_specific_spatial_pattern_source(country::BEF, spatPatInv.get_spatial_pattern(country::BEF, pol, EmissionSector(nfr)));
        }
    }

    const auto pointSourcesFlanders = read_point_sources(cfg, country::BEF, summary);

    const auto nfrTotalEmissions  = read_nfr_emissions(cfg, summary);
    const auto gnfrTotalEmissions = read_gnfr_emissions(cfg, summary);

    const ScalingFactors scalingsDiffuse;
    const ScalingFactors scalingsPointSource;

    //const auto scalingsDiffuse     = parse_scaling_factors(throw_if_not_exists(cfg.diffuse_scalings_path()), cfg.countries(), cfg.sectors(), cfg.pollutants());
    //const auto scalingsPointSource = parse_scaling_factors(throw_if_not_exists(cfg.point_source_scalings_path()), cfg.countries(), cfg.sectors(), cfg.pollutants());

    auto gridData = grid_data(cfg.grid_definition());

    Log::debug("Generate emission inventory");
    chrono::DurationRecorder dur;
    const auto inventory = create_emission_inventory(nfrTotalEmissions, gnfrTotalEmissions, pointSourcesFlanders, scalingsDiffuse, scalingsPointSource, summary);
    Log::debug("Generate emission inventory took {}", dur.elapsed_time_string());

    /*Log::debug("Spread emissions");
    dur.reset();
    spread_emissions(inventory, spatPatInv, cfg, *outputBuilder, progressCb);
    Log::debug("Spread emissions took {}", dur.elapsed_time_string());*/

    // Write the summary
    {
        const auto summaryPath = cfg.run_summary_path();
        file::create_directory_for_file(summaryPath);

        file::Handle fp(summaryPath, "wt");
        if (fp.is_open()) {
            fmt::fprintf(fp, "Used emissions\n");
            fmt::fprintf(fp, summary.emission_source_usage_table());
            fmt::fprintf(fp, "\nUsed spatial patterns\n");
            fmt::fprintf(fp, summary.spatial_pattern_usage_table());
        }

        summary.write_summary_spreadsheet(cfg.run_summary_spreadsheet_path());
    }
}
}
