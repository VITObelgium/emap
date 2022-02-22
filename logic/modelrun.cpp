#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/countryborders.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "emissioninventory.h"
#include "emissionscollector.h"
#include "gridrasterbuilder.h"
#include "outputwriters.h"
#include "runsummary.h"
#include "spatialpatterninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/log.h"

#include "gdx/algo/sum.h"
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
    auto runConfig = parse_run_configuration_file(runConfigPath);
    std::unique_ptr<inf::LogRegistration> logReg;
    inf::Log::add_file_sink(runConfig.output_path() / "emap.log");

    logReg = std::make_unique<inf::LogRegistration>("e-map");
    inf::Log::set_level(logLevel);

    return run_model(runConfig, progressCb);
}

static gdx::DenseRaster<double> apply_spatial_pattern_raster(const fs::path& rasterPath, const EmissionIdentifier& /*emissionId*/, double emissionValue, const CountryCellCoverage& countryCoverage)
{
    auto raster = extract_country_from_raster(rasterPath, countryCoverage);
    if (gdx::sum(raster) == 0.0) {
        // no spreading info, fall back to uniform spread needed
        return {};
    }

    auto outputGridRaster = gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average);
    normalize_raster(outputGridRaster);
    outputGridRaster *= emissionValue;

    /*if (emission.id().country.iso_code() == "AT" && emission.id().sector.name() == "1A1a") {
        auto outputGridRasterAt = gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average);
        gdx::write_raster(raster, fs::u8path(fmt::format("c:/temp/cams_{}.tif", emissionId)));
        gdx::write_raster(outputGridRasterAt, fs::u8path(fmt::format("c:/temp/vlops_pre_norm_{}.tif", emissionId)));
        normalize_raster(outputGridRasterAt);
        gdx::write_raster(outputGridRasterAt, fs::u8path(fmt::format("c:/temp/vlops_norm_{}.tif", emissionId)));
        gdx::write_raster(outputGridRaster, fs::u8path(fmt::format("c:/temp/vlops_spread_{}_{}.tif", emissionId, emission.scaled_diffuse_emissions())));
    }*/

    return outputGridRaster;
}

static gdx::DenseRaster<double> apply_spatial_pattern_ceip(const fs::path& ceipPath, const EmissionIdentifier& emissionId, double emissionValue, const inf::GeoMetadata& outputGrid, const RunConfiguration& cfg)
{
    // The returned raster is not normalized, it contains only values for the requested country
    auto raster = parse_spatial_pattern_ceip(ceipPath, emissionId, cfg);
    if (gdx::sum(raster) == 0.0) {
        // no spreading info, fall back to uniform spread need
        return {};
    }

    raster = gdx::resample_raster(raster, outputGrid, gdal::ResampleAlgorithm::Average);
    normalize_raster(raster);
    raster *= emissionValue;
    return raster;
}

static gdx::DenseRaster<double> apply_spatial_pattern_table(const fs::path& tablePath, const EmissionIdentifier& emissionId, double emissionValue, const RunConfiguration& cfg)
{
    if (emissionId.country == country::BEF) {
        auto raster = parse_spatial_pattern_flanders(tablePath, emissionId.sector, cfg);
        raster *= emissionValue;
        return raster;
    }

    throw RuntimeError("Spatial pattern tables are only implemented for Flanders, not {}", emissionId.country);
}

static gdx::DenseRaster<double> apply_uniform_spread(double emissionValue, const CountryCellCoverage& countryCoverage)
{
    return spread_values_uniformly_over_cells(emissionValue, countryCoverage);
}

static gdx::DenseRaster<double> apply_spatial_pattern(const SpatialPatternSource& spatialPattern, const EmissionIdentifier& emissionId, double emissionValue, const CountryCellCoverage& countryCoverage, const RunConfiguration& cfg)
{
    gdx::DenseRaster<double> result;
    switch (spatialPattern.type) {
    case SpatialPatternSource::Type::SpatialPatternCAMS:
        result = apply_spatial_pattern_raster(spatialPattern.path, emissionId, emissionValue, countryCoverage);
        break;
    case SpatialPatternSource::Type::SpatialPatternCEIP:
        result = apply_spatial_pattern_ceip(spatialPattern.path, emissionId, emissionValue, countryCoverage.outputSubgridExtent, cfg);
        break;
    case SpatialPatternSource::Type::SpatialPatternTable:
        result = apply_spatial_pattern_table(spatialPattern.path, emissionId, emissionValue, cfg);
        break;
    case SpatialPatternSource::Type::UnfiformSpread:
        result = apply_uniform_spread(emissionValue, countryCoverage);
        break;
    default:
        throw RuntimeError("Invalid spatial pattern type");
    }

    if (!result.empty() && result.metadata().cell_size_x() != countryCoverage.outputSubgridExtent.cell_size_x()) {
        throw std::logic_error("Spatial pattern grid resolution bug!");
    }

    if (result.empty() && emissionValue > 0) {
        // emission could not be spread, fall back to uniform spread
        Log::warn("No spatial pattern information available for {}: falling back to uniform spread", emissionId);
        result = apply_uniform_spread(emissionValue, countryCoverage);
    }

    return result;
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

static GeoMetadata metadata_with_modified_cellsize(const GeoMetadata meta, GeoMetadata::CellSize cellsize)
{
    GeoMetadata result = meta;
    result.rows /= truncate<int32_t>(cellsize.y / result.cell_size_y());
    result.cols /= truncate<int32_t>(cellsize.x / result.cell_size_x());
    result.cellSize = cellsize;
    return result;
}

void spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    Log::debug("Create country coverages");

    const auto gridDefinitions = grids_for_model_grid(cfg.model_grid());

    CPLSetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "TRUE");
    CountryBorders countryBorders(cfg.countries_vector_path(), cfg.country_field_id(), grid_data(gridDefinitions.front()).meta, cfg.countries());

    // A map that contains per country the remaining emission value that needs to be spread on a higher resolution
    std::unordered_map<EmissionIdentifier, double> remainingEmissions;

    for (auto gridIter = gridDefinitions.begin(); gridIter != gridDefinitions.end(); ++gridIter) {
        bool isCoursestGrid = gridIter == gridDefinitions.begin();

        auto& gridDefinition = *gridIter;
        auto& gridData       = grid_data(gridDefinition);

        std::optional<GeoMetadata> subGridMeta;
        if (auto nextIter = gridIter + 1; nextIter != gridDefinitions.end()) {
            subGridMeta = metadata_with_modified_cellsize(grid_data(*nextIter).meta, gridData.meta.cellSize);
        }

        ModelProgressInfo progressInfo;
        ProgressTracker progress(countryBorders.known_countries_in_extent(gridData.meta), progressCb);

        // Precompute the cell coverages per country as it can be expensive
        chrono::DurationRecorder dur;
        const auto countryCoverages = countryBorders.create_country_coverages(gridData.meta, [&](const GridProcessingProgress::ProgressTracker::Status& status) {
            progressInfo.info = fmt::format("Calculate region cells: {}", status.payload().full_name());
            progress.set_payload(progressInfo);
            progress.tick();
            return ProgressStatusResult::Continue;
        });

        Log::debug("Create country coverages took {}", dur.elapsed_time_string());

        progress.reset(cfg.pollutants().list().size() * cfg.sectors().nfr_sectors().size());

        for (const auto& pollutant : cfg.pollutants().list()) {
            EmissionsCollector collector(cfg, pollutant, gridData);

            for (const auto& sector : cfg.sectors().nfr_sectors()) {
                ModelProgressInfo info;
                info.info = fmt::format("[{}] Spread {} emissions for sector '{}'", gridData.name, pollutant, sector.code());
                progress.set_payload(info);
                progress.tick();

                // double spreadEmissionTotal = 0.0;

                // std::for_each(countryCoverages.begin(), countryCoverages.end(), [&](const CountryCellCoverage& cellCoverageInfo) {
                tbb::parallel_for_each(countryCoverages, [&](const CountryCellCoverage& cellCoverageInfo) {
                    if (cellCoverageInfo.country.is_belgium()) {
                        return;
                    }

                    try {
                        EmissionIdentifier emissionId(cellCoverageInfo.country, EmissionSector(sector), pollutant);

                        double emissionToSpread = 0.0;
                        if (isCoursestGrid) {
                            const auto emissions = emissionInv.emissions_with_id(emissionId);
                            if (emissions.empty()) {
                                Log::debug("No emissions available for pollutant {} in sector: {} in {}", pollutant, EmissionSector(sector), cellCoverageInfo.country);
                            } else if (emissions.size() > 1) {
                                Log::debug("Multiple emissions available for pollutant {} in sector: {} in {}", pollutant, EmissionSector(sector), cellCoverageInfo.country);
                            } else {
                                emissionToSpread = emissions.front().scaled_diffuse_emissions();
                            }
                        } else {
                            std::scoped_lock lock;
                            if (auto* remainingEmission = find_in_map(remainingEmissions, emissionId); remainingEmission != nullptr) {
                                emissionToSpread = *remainingEmission;
                            } else {
                                emissionToSpread = 0.0;
                            }
                        }

                        if (emissionToSpread == 0.0) {
                            return;
                        }

                        auto countryRaster = apply_spatial_pattern(spatialPatternInv.get_spatial_pattern(emissionId), emissionId, emissionToSpread, cellCoverageInfo, cfg);
                        if (countryRaster.empty()) {
                            return;
                        }

                        double erasedEmission = 0.0;
                        if (subGridMeta.has_value()) {
                            // Erase the region in the subgrid for which we will perform a higher resolution calculation
                            erasedEmission = erase_area_in_raster_and_sum_erased_values(countryRaster, *subGridMeta);
                            if (erasedEmission > 0) {
                                std::scoped_lock lock;
                                remainingEmissions[emissionId] = erasedEmission;
                            }
                        }

                        collector.add_diffuse_emissions(emissionId.country, sector, std::move(countryRaster));
                    } catch (const std::exception& e) {
                        Log::error("Error spreading emission: {}", e.what());
                    }
                });

                // if (gridBuilder.has_value()) {
                //     /*if (auto sum = gridBuilder->current_sum(); std::abs(sum - spreadEmissionTotal) > 0.01) {
                //         Log::warn("Big difference in spread emissions and emissions sum: spread={} - sum={} (diff: {})", spreadEmissionTotal, sum, std::abs(sum - spreadEmissionTotal));
                //     }*/

                //    gridBuilder->write_to_disk(cfg.output_path_for_grid_raster(pollutant, EmissionSector(sector), gridData));
                //}
            }

            // outputBuilder->write_to_disk(cfg, isCoursestGrid ? IOutputBuilder::WriteMode::Create : IOutputBuilder::WriteMode::Append);
            collector.write_to_disk(isCoursestGrid ? EmissionsCollector::WriteMode::Create : EmissionsCollector::WriteMode::Append);
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
                            throw RuntimeError("Invalid PM data for sector {} with EIL nr {} (PM10: {}, PM2.5 {})", pm10Entry.id().sector, pm10Entry.source_id(), *pm10Val, *pm2_5Val);
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

    // if (fs::exists(cfg.output_path())) {
    //     Log::debug("Clean output directory");
    //     try {
    //         fs::remove_all(cfg.output_path());
    //     } catch (const fs::filesystem_error& e) {
    //         Log::error(e.what());
    //         throw RuntimeError("Failed to clean up existing output directory, make sure none of the files are opened");
    //     }
    //     Log::debug("Output directory cleaned up");
    // }

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

    ScalingFactors scalingsDiffuse;
    ScalingFactors scalingsPointSource;

    if (auto path = cfg.diffuse_scalings_path(); fs::exists(path)) {
        scalingsDiffuse = parse_scaling_factors(path, cfg);
    }

    if (auto path = cfg.point_source_scalings_path(); fs::exists(path)) {
        scalingsDiffuse = parse_scaling_factors(path, cfg);
    }

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
