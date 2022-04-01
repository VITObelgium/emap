#include "emap/modelrun.h"

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

int run_model(const fs::path& runConfigPath, inf::Log::Level logLevel, std::optional<int32_t> concurrency, const ModelProgress::Callback& progressCb)
{
    auto runConfig = parse_run_configuration_file(runConfigPath);
    runConfig.set_max_concurrency(concurrency);
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

    return outputGridRaster;
}

static gdx::DenseRaster<double> apply_spatial_pattern_raster_without_cell_adjustments(const fs::path& rasterPath, const EmissionIdentifier& /*emissionId*/, double emissionValue, const CountryCellCoverage& countryCoverage)
{
    auto raster = gdx::read_dense_raster<double>(rasterPath, countryCoverage.outputSubgridExtent);
    if (gdx::sum(raster) == 0.0) {
        // no spreading info, fall back to uniform spread needed
        return {};
    }

    auto outputGridRaster = gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average);
    normalize_raster(outputGridRaster);
    outputGridRaster *= emissionValue;

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

static gdx::DenseRaster<double> apply_spatial_pattern_flanders(const gdx::DenseRaster<double>& pattern, double emissionValue, const inf::GeoMetadata& outputGrid)
{
    if (gdx::sum(pattern) == 0.0) {
        // no spreading info, fall back to uniform spread needed
        return {};
    }

    auto outputGridRaster = gdx::resample_raster(pattern, outputGrid, gdal::ResampleAlgorithm::Average);
    normalize_raster(outputGridRaster);
    outputGridRaster *= emissionValue;

    return outputGridRaster;
}

static gdx::DenseRaster<double> apply_uniform_spread(double emissionValue, const CountryCellCoverage& countryCoverage)
{
    return spread_values_uniformly_over_cells(emissionValue, countryCoverage);
}

enum class SpatialPatternStatus
{
    Ok,
    NoEmissionToSpread,
    FallbackToUniformSpread,
};

static gdx::DenseRaster<double> apply_spatial_pattern(const SpatialPatternSource& spatialPattern, const EmissionIdentifier& emissionId, double emissionValue, const CountryCellCoverage& countryCoverage, const RunConfiguration& cfg, SpatialPatternStatus& status)
{
    gdx::DenseRaster<double> result;

    if (emissionValue == 0) {
        status = SpatialPatternStatus::NoEmissionToSpread;
        return result;
    }

    switch (spatialPattern.type) {
    case SpatialPatternSource::Type::SpatialPatternCAMS:
    case SpatialPatternSource::Type::Raster:
        result = apply_spatial_pattern_raster(spatialPattern.path, emissionId, emissionValue, countryCoverage);
        break;
    case SpatialPatternSource::Type::SpatialPatternCEIP:
        result = apply_spatial_pattern_ceip(spatialPattern.path, emissionId, emissionValue, countryCoverage.outputSubgridExtent, cfg);
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

    if (result.empty()) {
        // emission could not be spread, fall back to uniform spread
        Log::debug("No spatial pattern information available for {}: falling back to uniform spread", emissionId);
        result = apply_uniform_spread(emissionValue, countryCoverage);
        status = SpatialPatternStatus::FallbackToUniformSpread;
    } else {
        status = SpatialPatternStatus::Ok;
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

static void spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, EmissionValidation* validator, RunSummary& summary, const ModelProgress::Callback& progressCb)
{
    chrono::ScopedDurationLog d("Spread emissions");

    const auto gridDefinitions = grids_for_model_grid(cfg.model_grid());

    CPLSetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "TRUE");
    CountryBorders countryBorders(cfg.boundaries_vector_path(), cfg.boundaries_field_id(), grid_data(gridDefinitions.front()).meta, cfg.countries());
    CountryBorders eezCountryBorders(cfg.eez_boundaries_vector_path(), cfg.eez_boundaries_field_id(), grid_data(gridDefinitions.front()).meta, cfg.countries());

    // A map that contains per country the remaining emission value that needs to be spread on a higher resolution
    std::unordered_map<EmissionIdentifier, double> remainingEmissions;

    EmissionsCollector collector(cfg);

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

        const auto eezCountryCoverages = eezCountryBorders.create_country_coverages(gridData.meta, [&](const GridProcessingProgress::ProgressTracker::Status& status) {
            progressInfo.info = fmt::format("Calculate eez region cells: {}", status.payload().full_name());
            progress.set_payload(progressInfo);
            progress.tick();
            return ProgressStatusResult::Continue;
        });

        Log::debug("Create country coverages took {}", dur.elapsed_time_string());

        progress.reset(cfg.included_pollutants().size() * cfg.sectors().nfr_sectors().size());
        std::mutex mut;

        for (const auto& pollutant : cfg.included_pollutants()) {
            collector.start_pollutant(pollutant, gridData);

            for (const auto& sector : cfg.sectors().nfr_sectors()) {
                ModelProgressInfo info;
                info.info = fmt::format("[{}] Spread {} emissions for sector '{}'", gridData.name, pollutant, sector.code());
                progress.set_payload(info);
                progress.tick();

                const auto& sectorCoverages = sector.destination() == EmissionDestination::Eez ? eezCountryCoverages : countryCoverages;

                // std::for_each(sectorCoverages.begin(), sectorCoverages.end(), [&](const CountryCellCoverage& cellCoverageInfo) {
                tbb::parallel_for_each(sectorCoverages, [&](const CountryCellCoverage& cellCoverageInfo) {
                    if (cellCoverageInfo.country == country::BEF) {
                        return;
                    }

                    /*if (cellCoverageInfo.country.iso_code() != "GB" || sector.name() != "2C7c" || pollutant.code() != "Hg") {
                        return;
                    }*/

                    try {
                        EmissionIdentifier emissionId(cellCoverageInfo.country, EmissionSector(sector), pollutant);

                        const auto emission = emissionInv.try_emission_with_id(emissionId);
                        if (!emission.has_value()) {
                            return;
                        }

                        double emissionToSpread = 0.0;
                        if (isCoursestGrid) {
                            emissionToSpread = emission->scaled_diffuse_emissions_sum();
                        } else {
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

                        SpatialPatternStatus spatPatStatus;
                        const auto spatialPattern = spatialPatternInv.get_spatial_pattern(emissionId);
                        auto countryRaster        = apply_spatial_pattern(spatialPattern, emissionId, emissionToSpread, cellCoverageInfo, cfg, spatPatStatus);
                        if (isCoursestGrid) {
                            if (spatPatStatus == SpatialPatternStatus::FallbackToUniformSpread) {
                                summary.add_spatial_pattern_source_without_data(spatialPattern, emission->scaled_total_emissions_sum(), emission->point_emission_sum());
                            } else {
                                summary.add_spatial_pattern_source(spatialPattern, emission->scaled_total_emissions_sum(), emission->point_emission_sum());
                            }
                        }

                        if (countryRaster.empty()) {
                            return;
                        }

                        double erasedEmission = 0.0;
                        if (subGridMeta.has_value()) {
                            // Erase the region in the subgrid for which we will perform a higher resolution calculation
                            erasedEmission = erase_area_in_raster_and_sum_erased_values(countryRaster, *subGridMeta);
                            std::scoped_lock lock(mut);
                            if (erasedEmission > 0) {
                                remainingEmissions[emissionId] = erasedEmission;
                            } else {
                                remainingEmissions.erase(emissionId);
                            }
                        }

                        if (validator) {
                            validator->add_diffuse_emissions(emissionId, countryRaster);
                        }

                        // Add the point sources to the grid
                        auto pointEmissions = container_as_vector(emission->point_emissions());
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
                            collector.add_emissions(emissionId.country, sector, std::move(countryRaster), emission->scaled_point_emissions());
                            if (validator) {
                                validator->add_point_emissions(emissionId, emission->scaled_point_emissions_sum());
                            }
                        } else {
                            collector.add_emissions(emissionId.country, sector, std::move(countryRaster), {});
                        }
                    } catch (const std::exception& e) {
                        Log::error("Error spreading emission: {}", e.what());
                    }
                });
            }

            if (gridIter + 1 == gridDefinitions.end()) {
                // Now do flanders (for finest grid)
                SpatialPatternTableCache cache(cfg);
                const auto sectors = cfg.sectors().nfr_sectors();
                // std::for_each(sectors.begin(), sectors.end(), [&](const NfrSector& sector) {
                tbb::parallel_for_each(sectors, [&](const NfrSector& sector) {
                    EmissionIdentifier emissionId(country::BEF, EmissionSector(sector), pollutant);

                    const auto& sectorCoverages  = sector.destination() == EmissionDestination::Eez ? eezCountryCoverages : countryCoverages;
                    const auto& flandersCoverage = find_in_container_required(sectorCoverages, [](const CountryCellCoverage& cov) {
                        return cov.country == country::BEF;
                    });

                    auto spatialPattern = spatialPatternInv.get_spatial_pattern(emissionId, &cache);
                    auto emission       = emissionInv.try_emission_with_id(emissionId);
                    if (!emission.has_value()) {
                        return;
                    }

                    gdx::DenseRaster<double> raster;
                    const auto diffuseEmissions = emission->scaled_diffuse_emissions_sum();
                    if (spatialPattern.type == SpatialPatternSource::Type::SpatialPatternTable) {
                        const auto* spatPat = cache.get_data(spatialPattern.path, emissionId.with_pollutant(spatialPattern.usedEmissionId.pollutant));
                        if (spatPat != nullptr) {
                            raster = apply_spatial_pattern_flanders(spatPat->raster, diffuseEmissions, gridData.meta);
                        }
                    } else if (spatialPattern.type == SpatialPatternSource::Type::Raster) {
                        raster = apply_spatial_pattern_raster(spatialPattern.path, emission->id(), diffuseEmissions, flandersCoverage);
                    } else if (spatialPattern.type == SpatialPatternSource::Type::UnfiformSpread) {
                        raster = apply_uniform_spread(diffuseEmissions, flandersCoverage);
                    }

                    if (raster.empty()) {
                        Log::debug("No spatial pattern information available for {}: falling back to uniform spread", emissionId);
                        raster = apply_uniform_spread(diffuseEmissions, flandersCoverage);
                        summary.add_spatial_pattern_source_without_data(spatialPattern, emission->scaled_total_emissions_sum(), emission->point_emission_sum());
                    } else {
                        summary.add_spatial_pattern_source(spatialPattern, emission->scaled_total_emissions_sum(), emission->point_emission_sum());
                    }

                    if (validator) {
                        validator->add_diffuse_emissions(emissionId, raster);
                        validator->add_point_emissions(emissionId, emission->scaled_point_emissions_sum());
                    }

                    collector.add_emissions(emissionId.country, sector, std::move(raster), emission->scaled_point_emissions());
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
                fs::remove_all(entry.path());
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
        validator = std::make_unique<EmissionValidation>();
    }

    return validator;
}

int run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    try {
        tbb::global_control tbbControl(tbb::global_control::max_allowed_parallelism, cfg.max_concurrency().value_or(oneapi::tbb::info::default_concurrency()));

        RunSummary summary(cfg);

        SpatialPatternInventory spatPatInv(cfg);
        spatPatInv.scan_dir(cfg.reporting_year(), cfg.year(), cfg.spatial_pattern_path());

        clean_output_directory(cfg.output_path());

        auto validator       = make_validator(cfg);
        const auto inventory = make_emission_inventory(cfg, summary);
        spread_emissions(inventory, spatPatInv, cfg, validator.get(), summary, progressCb);

        {
            chrono::ScopedDurationLog d("Write model run summary");
            if (validator) {
                summary.set_validation_results(validator->create_summary(inventory));
            }

            summary.write_summary(cfg.output_path());
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        Log::error(e.what());
        fmt::print("{}\n", e.what());
        return EXIT_FAILURE;
    }
}
}