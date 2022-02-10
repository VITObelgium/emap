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

static gdx::DenseRaster<double> apply_spatial_pattern_raster(const fs::path& rasterPath, const EmissionInventoryEntry& emission, const CountryCellCoverage& countryCoverage)
{
    // The returned raster is normalized
    auto raster = extract_country_from_raster(rasterPath, countryCoverage);
    raster *= emission.diffuse_emissions();

    if (emission.id().country.iso_code() == "AT" && emission.id().sector.name() == "1A1a") {
        gdx::write_raster(raster, fs::u8path(fmt::format("c:/temp/cams_{}.tif", emission.id())));

        int expand = 10;

        auto testExtent = countryCoverage.outputSubgridExtent;
        testExtent.xll -= testExtent.cell_size_x() * expand;
        testExtent.yll += testExtent.cell_size_y() * expand;
        testExtent.cols += expand * 2;
        testExtent.rows += expand * 2;

        gdx::write_raster(gdx::resample_raster(raster, testExtent, gdal::ResampleAlgorithm::Sum), fs::u8path(fmt::format("c:/temp/vlops_{}.tif", emission.id())));
    }

    return gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Sum);
}

static gdx::DenseRaster<double> apply_spatial_pattern_ceip(const fs::path& ceipPath, const EmissionInventoryEntry& emission, const GridData& grid, const RunConfiguration& cfg)
{
    // The returned raster is not normalized, it contains only values for the requested country
    auto raster = parse_spatial_pattern_ceip(ceipPath, emission.id(), cfg);
    normalize_raster(raster);
    raster *= emission.diffuse_emissions();

    return gdx::resample_raster(raster, grid.meta, gdal::ResampleAlgorithm::Sum);
}

static gdx::DenseRaster<double> apply_spatial_pattern_table(const fs::path& tablePath, const EmissionInventoryEntry& emission, const RunConfiguration& cfg)
{
    if (emission.id().country == country::BEF) {
        return parse_spatial_pattern_flanders(tablePath, emission.id().sector, cfg);
    }

    throw RuntimeError("Spatial pattern tables are only implemented for Flanders, not {}", emission.id().country);
}

static gdx::DenseRaster<double> apply_uniform_spread(const EmissionInventoryEntry& emission, const CountryCellCoverage& countryCoverage)
{
    // Cell coverage are based on the CAMS grid
    return gdx::resample_raster(spread_values_uniformly_over_cells(emission.scaled_diffuse_emissions(), countryCoverage), countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Sum);
}

static gdx::DenseRaster<double> apply_spatial_pattern(const SpatialPatternSource& spatialPattern, const EmissionInventoryEntry& emission, const GridData& grid, const CountryCellCoverage& countryCoverage, const RunConfiguration& cfg)
{
    switch (spatialPattern.type) {
    case SpatialPatternSource::Type::SpatialPatternCAMS:
        return apply_spatial_pattern_raster(spatialPattern.path, emission, countryCoverage);
    case SpatialPatternSource::Type::SpatialPatternCEIP:
        return apply_spatial_pattern_ceip(spatialPattern.path, emission, grid, cfg);
    case SpatialPatternSource::Type::SpatialPatternTable:
        return apply_spatial_pattern_table(spatialPattern.path, emission, cfg);
    case SpatialPatternSource::Type::UnfiformSpread:
        return apply_uniform_spread(emission, countryCoverage);
    }

    throw RuntimeError("Invalid spatial pattern type");
}

static fs::path create_vlops_output_name(const Pollutant& pol, date::year year, std::string_view suffix)
{
    auto filename = fmt::format("{}_OPS_{}", pol.code(), static_cast<int32_t>(year));
    if (!suffix.empty()) {
        filename += suffix;
    }
    filename += ".brn";

    return fs::u8path(filename);
}

static std::vector<Country> countries_that_use_configured_grid(std::span<const Country> countries)
{
    std::vector<Country> result;

    for (auto& country : countries) {
        if (!country.is_belgium()) {
            result.push_back(country);
        }
    }

    return result;
}

void spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    const auto spatialPatternGrid = grid_data(GridDefinition::CAMS);
    const auto outputGrid         = grid_data(cfg.grid_definition());

    Log::debug("Create country coverages");

    ModelProgressInfo progressInfo;
    ProgressTracker progress(known_countries_in_extent(cfg.countries(), spatialPatternGrid.meta, cfg.countries_vector_path(), cfg.country_field_id()), progressCb);

    // Precompute the cell coverages per country as it can be expensive
    chrono::DurationRecorder dur;
    const auto countryCoverages = create_country_coverages(spatialPatternGrid.meta, outputGrid.meta, cfg.countries_vector_path(), cfg.country_field_id(), cfg.countries(), [&](const GridProcessingProgress::ProgressTracker::Status& status) {
        progressInfo.info = fmt::format("Calculate region cells: {}", status.payload().full_name());
        progress.set_payload(progressInfo);
        progress.tick();
        return ProgressStatusResult::Continue;
    });
    Log::debug("Create country coverages took {}", dur.elapsed_time_string());

    //const auto gridCountries = countries_that_use_configured_grid(cfg.countries().list());
    progress.reset((countryCoverages.size() - 3) * cfg.pollutants().list().size() * cfg.sectors().nfr_sectors().size());

    for (const auto& pollutant : cfg.pollutants().list()) {
        auto outputBuilder = make_output_builder(cfg);

        for (const auto& cellCoverageInfo : countryCoverages) {
            if (cellCoverageInfo.country.is_belgium()) {
                continue;
            }

            tbb::parallel_for_each(cfg.sectors().nfr_sectors(), [&](const auto& sector) {
                try {
                    ModelProgressInfo info;
                    info.info = fmt::format("Spread {} emissions in {} for sector '{}'", pollutant, cellCoverageInfo.country.full_name(), sector.code());
                    progress.set_payload(info);
                    progress.tick();

                    EmissionIdentifier emissionId(cellCoverageInfo.country, EmissionSector(sector), pollutant);

                    const auto emissions = emissionInv.emissions_with_id(emissionId);
                    if (emissions.empty()) {
                        Log::debug("No emissions available for pollutant {} in sector: {} in {}", pollutant, EmissionSector(sector), cellCoverageInfo.country);
                        return;
                    } else if (emissions.size() > 1) {
                        Log::debug("Multiple emissions available for pollutant {} in sector: {} in {}", pollutant, EmissionSector(sector), cellCoverageInfo.country);
                        return;
                    }

                    const auto resultRaster = apply_spatial_pattern(spatialPatternInv.get_spatial_pattern(emissionId), emissions.front(), grid_data(cfg.grid_definition()), cellCoverageInfo, cfg);
                    const auto& meta        = resultRaster.metadata();

                    for (auto cell : gdx::RasterCells(resultRaster)) {
                        if (resultRaster.is_nodata(cell) || resultRaster[cell] == 0.0) {
                            continue;
                        }

                        const auto cellCenter = meta.convert_cell_centre_to_xy(cell);
                        outputBuilder->add_diffuse_output_entry(emissionId, truncate<int64_t>(cellCenter.x), truncate<int64_t>(cellCenter.y), resultRaster[cell]);
                    }

                    if (cfg.output_tifs()) {
                        gdx::write_raster(resultRaster, cfg.output_path_for_tif(emissionId));
                    }
                } catch (const std::exception& e) {
                    Log::error("Error spreading emission: {}", e.what());
                }
            });
        }

        outputBuilder->write_to_disk(cfg);
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
                            throw RuntimeError("Invalid PM data for sector {} (PM10: {}, PM2.5 {})", pm10Entry.id().sector, *pm10Val, *pm2_5Val);
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

    Log::debug("Spread emissions");
    dur.reset();
    spread_emissions(inventory, spatPatInv, cfg, progressCb);
    Log::debug("Spread emissions took {}", dur.elapsed_time_string());

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
