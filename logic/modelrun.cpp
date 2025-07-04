﻿#include "emap/modelrun.h"

#include "emapconfig.h"

#include "emap/configurationparser.h"
#include "emap/constants.h"
#include "emap/countryborders.h"
#include "emap/emissioninventory.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "emissionscollector.h"
#include "emissionvalidation.h"
#include "gridrasterbuilder.h"
#include "outputwriters.h"
#include "runsummary.h"
#include "spatialpatterninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/math.h"

#include "gdx/algo/sum.h"
#include "gdx/denserasterio.h"

#include <numeric>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/parallel_pipeline.h>
#include <unordered_set>

namespace emap {

using namespace inf;
namespace gdal = inf::gdal;

struct SpatialPatternProcessInfo
{
    enum class Status
    {
        Ok,
        NoEmissionToSpread,
        FallbackToUniformSpread,
    };

    double emissions_outside_of_the_grid() const noexcept
    {
        return diffuseEmissions - emissionsWithinOutput;
    }

    Status status                = Status::Ok;
    double diffuseEmissions      = 0.0;
    double emissionsWithinOutput = 0.0;
};

static SpatialPatternProcessInfo apply_emission_to_spatial_pattern(SpatialPattern& spatialPattern,
                                                                   double emissionValue,
                                                                   const GeoMetadata& outputExtent,
                                                                   const CountryCellCoverage& countryCoverage)
{
    SpatialPatternProcessInfo info;
    info.diffuseEmissions = emissionValue;

    if (emissionValue == 0) {
        info.status           = SpatialPatternProcessInfo::Status::NoEmissionToSpread;
        spatialPattern.raster = {};
        return info;
    }

    info.status = SpatialPatternProcessInfo::Status::Ok;

    if (spatialPattern.source.type == SpatialPatternSource::Type::UnfiformSpread) {
        assert(spatialPattern.raster.empty());
        if (spatialPattern.source.patternAvailableButWithoutData) {
            info.status = SpatialPatternProcessInfo::Status::FallbackToUniformSpread;
        }

        spatialPattern.raster = spread_values_uniformly_over_cells(emissionValue, countryCoverage);
    } else {
        if (spatialPattern.raster.empty()) {
            throw std::logic_error("Raster should not be empty");
        }

        // Spatial pattern data is available and will contain data
        spatialPattern.raster *= emissionValue;
    }

    const auto& countryExtent = spatialPattern.raster.metadata();
    const auto intersection   = metadata_intersection(countryExtent, outputExtent);
    if (intersection.bounding_box() != countryExtent.bounding_box()) {
        // country extent is outside of the output grid
        spatialPattern.raster = gdx::sub_raster(spatialPattern.raster, intersection);
    }

    info.emissionsWithinOutput = spatialPattern.raster.sum();

    return info;
}

static GeoMetadata metadata_with_modified_cellsize(const GeoMetadata meta, GeoMetadata::CellSize cellsize)
{
    GeoMetadata result = meta;
    result.rows /= truncate<int32_t>(cellsize.y / result.cell_size_y());
    result.cols /= truncate<int32_t>(cellsize.x / result.cell_size_x());
    result.cellSize = cellsize;
    return result;
}

static void spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, EmissionValidation* validator, RunSummary& summary, const ModelProgress::Callback& progressCb)
{
    chrono::ScopedDurationLog d("Spread emissions");

    const auto gridDefinitions = grids_for_model_grid(cfg.model_grid());

    // Clip the boundaries on the CAMS grid, we do not want to consider country geometries outside of the cams grid
    auto clipExtent = gdal::warp_metadata(grid_data(GridDefinition::CAMS).meta, grid_data(gridDefinitions.front()).meta.projection);

    CPLSetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "TRUE");
    CountryBorders countryBorders(cfg.boundaries_vector_path(), cfg.boundaries_field_id(), clipExtent, cfg.countries());
    CountryBorders eezCountryBorders(cfg.eez_boundaries_vector_path(), cfg.eez_boundaries_field_id(), clipExtent, cfg.countries());

    if (validator) {
        validator->set_grid_countries(countryBorders.known_countries_in_extent(grid_data(gridDefinitions.front()).meta));
    }

    // A map that contains per country the remaining emission value that needs to be spread on a higher resolution
    std::unordered_map<EmissionIdentifier, double> remainingEmissions;

    std::unordered_set<EmissionIdentifier> spatialPatternsCoursestGridUniformFallback;

    EmissionsCollector collector(cfg);

    for (auto gridIter = gridDefinitions.begin(); gridIter != gridDefinitions.end(); ++gridIter) {
        bool isCoursestGrid = gridIter == gridDefinitions.begin();

        // Current grid definition
        auto& gridDefinition = *gridIter;
        auto& gridData       = grid_data(gridDefinition);

        // Obtain the grid of the upcoming subgrid with finer resolution if it is available
        std::optional<GeoMetadata> subGridMeta;
        if (auto nextIter = gridIter + 1; nextIter != gridDefinitions.end()) {
            subGridMeta = metadata_with_modified_cellsize(grid_data(*nextIter).meta, gridData.meta.cellSize);
        }

        ModelProgressInfo progressInfo;
        ProgressTracker progress(countryBorders.known_countries_in_extent(gridData.meta).size(), progressCb);

        // Precompute the cell coverages per country as it can be expensive
        chrono::DurationRecorder dur;
        const auto coverageMode     = isCoursestGrid ? CoverageMode::AllCountryCells : CoverageMode::GridCellsOnly;
        const auto countryCoverages = countryBorders.create_country_coverages(gridData.meta, coverageMode, [&](const GridProcessingProgress::ProgressTracker::Status& status) {
            progressInfo.info = fmt::format("Calculate region cells: {}", status.payload().full_name());
            progress.set_payload(progressInfo);
            progress.tick();
            return ProgressStatusResult::Continue;
        });

        const auto eezCountryCoverages = eezCountryBorders.create_country_coverages(gridData.meta, coverageMode, [&](const GridProcessingProgress::ProgressTracker::Status& status) {
            progressInfo.info = fmt::format("Calculate eez region cells: {}", status.payload().full_name());
            progress.set_payload(progressInfo);
            progress.tick();
            return ProgressStatusResult::Continue;
        });

        if (countryCoverages.empty()) {
            throw RuntimeError("Unexpected country data: no country intersections found for grid '{}'", gridData.name);
        }

        Log::debug("Create country coverages took {}", dur.elapsed_time_string());

        progress.reset(cfg.included_pollutants().size() * cfg.sectors().nfr_sectors().size());
        std::mutex mut;

        for (const auto& pollutant : cfg.included_pollutants()) {
            collector.start_pollutant(pollutant, gridData);

            for (const auto& sector : cfg.sectors().nfr_sectors()) {
                ModelProgressInfo info;
                info.info = fmt::format("[{}] Spread {} for '{}'", gridData.name, pollutant, sector.code());
                progress.set_payload(info);
                progress.tick();

                const auto& sectorCoverages = sector.destination() == EmissionDestination::Eez ? eezCountryCoverages : countryCoverages;

                // std::for_each(sectorCoverages.begin(), sectorCoverages.end(), [&](const CountryCellCoverage& cellCoverageInfo) {
                tbb::parallel_for_each(sectorCoverages, [&](const CountryCellCoverage& cellCoverageInfo) {
                    if (cellCoverageInfo.country == country::BEF) {
                        return;
                    }

                    if (cfg.sectors().is_ignored_sector(EmissionSector::Type::Nfr, sector.code(), cellCoverageInfo.country)) {
                        return;
                    }

                    try {
                        EmissionIdentifier emissionId(cellCoverageInfo.country, EmissionSector(sector), pollutant);

                        const auto emission = emissionInv.try_emission_with_id(emissionId);
                        if (!emission.has_value()) {
                            return;
                        }

                        double emissionToSpread = 0.0;
                        if (isCoursestGrid) {
                            // coursest grid, all emissions need to be spread
                            emissionToSpread = emission->scaled_diffuse_emissions_sum();
                        } else {
                            // subgrid, only the emissions that ended up in this grid on the previous level need to be spread
                            std::scoped_lock lock(mut);
                            if (auto* remainingEmission = find_in_map(remainingEmissions, emissionId); remainingEmission != nullptr) {
                                emissionToSpread = *remainingEmission;
                            } else {
                                emissionToSpread = 0.0;
                            }
                        }

                        if (emissionToSpread == 0.0 && emission->point_emissions().empty()) {
                            return;
                        }

                        SpatialPattern spatialPattern;
                        if (isCoursestGrid) {
                            // only check the spatial pattern grid contents for the coursest grid
                            spatialPattern = spatialPatternInv.get_spatial_pattern_checked(emissionId, cellCoverageInfo);
                            if (spatialPattern.source.patternAvailableButWithoutData) {
                                std::scoped_lock lock(mut);
                                // Store the fact that we fallback to uniform spread because of missing data
                                // This needs to be checked on finer resolutions because on finer resolutions the contents are
                                // no longer checked and there we allso need to fallback to uniform spread if we did on the coursest grid
                                spatialPatternsCoursestGridUniformFallback.insert(emissionId);
                            }
                        } else {
                            if (spatialPatternsCoursestGridUniformFallback.count(emissionId) > 0) {
                                // The coursest grid already fallbacked to uniform spread, so we do the same here
                                spatialPattern = SpatialPattern(SpatialPatternSource::create_with_uniform_spread(emissionId.country, emissionId.sector, pollutant, true));
                            } else {
                                spatialPattern = spatialPatternInv.get_spatial_pattern(emissionId, cellCoverageInfo);
                            }
                        }

                        // Write the output raster to disk if configured
                        if (cfg.output_spatial_pattern_rasters() && !spatialPattern.raster.empty()) {
                            gdx::write_raster(spatialPattern.raster, cfg.output_path_for_spatial_pattern_raster(emissionId, gridData));
                        }

                        const auto spatPatInfo = apply_emission_to_spatial_pattern(spatialPattern, emissionToSpread, gridData.meta, cellCoverageInfo);
                        if (isCoursestGrid) {
                            if (spatPatInfo.status == SpatialPatternProcessInfo::Status::FallbackToUniformSpread) {
                                summary.add_spatial_pattern_source_without_data(spatialPattern.source, spatPatInfo.diffuseEmissions, spatPatInfo.emissionsWithinOutput, *emission);
                            } else {
                                summary.add_spatial_pattern_source(spatialPattern.source, spatPatInfo.diffuseEmissions, spatPatInfo.emissionsWithinOutput, *emission);
                            }
                        }

                        if (spatialPattern.raster.empty()) {
                            return;
                        }

                        double erasedEmission = 0.0;
                        if (subGridMeta.has_value()) {
                            // Erase the region in the subgrid for which we will perform a higher resolution calculation
                            erasedEmission = erase_area_in_raster_and_sum_erased_values(spatialPattern.raster, *subGridMeta);
                            std::scoped_lock lock(mut);
                            if (erasedEmission > 0) {
                                remainingEmissions[emissionId] = erasedEmission;
                            } else {
                                remainingEmissions.erase(emissionId);
                            }
                        }

                        if (validator) {
                            validator->add_diffuse_emissions(emissionId, spatialPattern.raster, spatPatInfo.emissions_outside_of_the_grid());
                        }

                        // Add the point sources to the grid
                        auto pointEmissions = container_as_vector(emission->scaled_point_emissions());
                        if (subGridMeta.has_value()) {
                            // remove the points from the subGrid
                            remove_from_container(pointEmissions, [meta = *subGridMeta](const EmissionEntry& entry) {
                                if (!entry.coordinate().has_value()) {
                                    return true;
                                }

                                if (meta.is_on_map(*entry.coordinate())) {
                                    return true;
                                }

                                return false;
                            });
                        }

                        if (isCoursestGrid) {
                            // Only add the point emissions once for the coursest grid as they are resolution independent
                            collector.add_emissions(cellCoverageInfo, sector, std::move(spatialPattern.raster), emission->scaled_point_emissions());
                            if (validator) {
                                validator->add_point_emissions(emissionId, emission->scaled_point_emissions_sum());
                            }
                        } else {
                            collector.add_emissions(cellCoverageInfo, sector, std::move(spatialPattern.raster), {});
                        }
                    } catch (const std::exception& e) {
                        Log::error("Error spreading emission: {}", e.what());
                    }
                });
            }

            if (gridIter + 1 == gridDefinitions.end()) {
                // Now do flanders (for finest grid)
                const auto sectors = cfg.sectors().nfr_sectors();
                // std::for_each(sectors.begin(), sectors.end(), [&](const NfrSector& sector) {
                tbb::parallel_for_each(sectors, [&](const NfrSector& sector) {
                    EmissionIdentifier emissionId(country::BEF, EmissionSector(sector), pollutant);

                    if (cfg.sectors().is_ignored_sector(EmissionSector::Type::Nfr, sector.code(), country::BEF)) {
                        return;
                    }

                    const auto& sectorCoverages  = sector.destination() == EmissionDestination::Eez ? eezCountryCoverages : countryCoverages;
                    const auto& flandersCoverage = find_in_container_required(sectorCoverages, [](const CountryCellCoverage& cov) {
                        return cov.country == country::BEF;
                    });

                    auto emission = emissionInv.try_emission_with_id(emissionId);
                    if (!emission.has_value()) {
                        return;
                    }

                    auto spatialPattern         = spatialPatternInv.get_spatial_pattern_checked(emissionId, flandersCoverage);
                    const auto diffuseEmissions = emission->scaled_diffuse_emissions_sum();
                    if (cfg.output_spatial_pattern_rasters() && !spatialPattern.raster.empty()) {
                        gdx::write_raster(spatialPattern.raster, cfg.output_path_for_spatial_pattern_raster(emissionId, gridData));
                    }

                    const auto spatPatInfo = apply_emission_to_spatial_pattern(spatialPattern, diffuseEmissions, gridData.meta, flandersCoverage);

                    if (spatialPattern.source.patternAvailableButWithoutData) {
                        Log::debug("No spatial pattern information available for {}: falling back to uniform spread", emissionId);
                        summary.add_spatial_pattern_source_without_data(spatialPattern.source, spatPatInfo.diffuseEmissions, spatPatInfo.emissionsWithinOutput, *emission);
                    } else {
                        summary.add_spatial_pattern_source(spatialPattern.source, spatPatInfo.diffuseEmissions, spatPatInfo.emissionsWithinOutput, *emission);
                    }

                    if (validator) {
                        validator->add_diffuse_emissions(emissionId, spatialPattern.raster, spatPatInfo.emissions_outside_of_the_grid());
                        validator->add_point_emissions(emissionId, emission->scaled_point_emissions_sum());
                    }

                    if (spatPatInfo.status != SpatialPatternProcessInfo::Status::NoEmissionToSpread && spatialPattern.raster.empty()) {
                        throw RuntimeError("Raster should not be empty");
                    }

                    collector.add_emissions(flandersCoverage, sector, std::move(spatialPattern.raster), emission->scaled_point_emissions());
                });
            }

            collector.flush_pollutant_to_disk(isCoursestGrid ? EmissionsCollector::WriteMode::Create : EmissionsCollector::WriteMode::Append);
        }

        collector.final_flush_to_disk(isCoursestGrid ? EmissionsCollector::WriteMode::Create : EmissionsCollector::WriteMode::Append);
    }
}

static void clean_output_directory(const fs::path& p)
{
    if (!fs::exists(p)) {
        return;
    }

    Log::debug("Clean output directory");
    try {
        for (auto& entry : fs::directory_iterator(p)) {
            if (entry.is_regular_file()) {
                // Don't remove the log file we have in use
                if (entry.path().extension() != ".log") {
                    fs::remove(entry);
                }
            } else if (entry.is_directory()) {
                if (entry.path().stem() != "grids") {
                    fs::remove_all(entry.path());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        Log::error(e.what());
        throw RuntimeError("Failed to clean up existing output directory, make sure none of the files are opened");
    }

    Log::debug("Output directory cleaned up");
}

static std::unique_ptr<EmissionValidation> make_validator(const RunConfiguration& cfg)
{
    std::unique_ptr<EmissionValidation> validator;

    if (cfg.validation_type() == ValidationType::SumValidation) {
        validator = std::make_unique<EmissionValidation>(cfg);
    }

    return validator;
}

int run_model(const fs::path& runConfigPath, inf::Log::Level logLevel, std::optional<int32_t> concurrency, const ModelProgress::Callback& progressCb)
{
    // Parse the configuration file
    auto runConfig = parse_run_configuration_file(runConfigPath);
    runConfig.set_max_concurrency(concurrency);

    // Configure logging
    inf::Log::add_file_sink(runConfig.output_path() / "emap.log");
    auto logReg = std::make_unique<inf::LogRegistration>("e-map");
    inf::Log::set_level(logLevel);

    Log::info("E-MAP {} ({})", EMAP_VERSION, EMAP_COMMIT_HASH);

    return run_model(runConfig, progressCb);
}

int run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    try {
        // configure the maximum allowed concurrency
        tbb::global_control tbbControl(tbb::global_control::max_allowed_parallelism, cfg.max_concurrency().value_or(oneapi::tbb::info::default_concurrency()));

        // data structure that contains all the summary information of the current run
        RunSummary summary(cfg);

        // scan the available spatial patterns for the configured year
        SpatialPatternInventory spatPatInv(cfg);
        spatPatInv.scan_dir(cfg.reporting_year(), cfg.year(), cfg.spatial_pattern_path());

        // remove existing results in the output directory
        clean_output_directory(cfg.output_path());

        // create a validator that compares incoming emissions to outgoing emissions (empty if validation is not enabled)
        auto validator = make_validator(cfg);

        // create the emission inventory that will contain all the emissions for the configured year
        const auto inventory = make_emission_inventory(cfg, summary);

        // spread the emissions from the inventory based on the spatial spatterns
        spread_emissions(inventory, spatPatInv, cfg, validator.get(), summary, progressCb);

        // write the model summary
        if (validator) {
            summary.set_validation_results(validator->create_summary(inventory));
        }
        summary.write_summary(cfg.output_path());

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        Log::error(e.what());
        fmt::print("{}\n", e.what());
        return EXIT_FAILURE;
    }
}
}
