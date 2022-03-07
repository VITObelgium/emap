#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/countryborders.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "emissioninventory.h"
#include "emissionscollector.h"
#include "emissionvalidation.h"
#include "gridrasterbuilder.h"
#include "outputwriters.h"
#include "runsummary.h"
#include "spatialpatterninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"

#include "gdx/algo/sum.h"
#include "gdx/denserasterio.h"

#include <numeric>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/parallel_pipeline.h>
#include <unordered_set>

namespace emap {

using namespace inf;

static fs::path throw_if_not_exists(fs::path&& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return std::move(path);
}

void run_model(const fs::path& runConfigPath, inf::Log::Level logLevel, std::optional<int32_t> concurrency, const ModelProgress::Callback& progressCb)
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

static gdx::DenseRaster<double> apply_spatial_pattern_table(const fs::path& tablePath, const EmissionIdentifier& emissionId, double emissionValue, const inf::GeoMetadata& outputGrid, const RunConfiguration& cfg)
{
    if (emissionId.country == country::BEF) {
        auto raster = parse_spatial_pattern_flanders(tablePath, emissionId.sector, cfg);
        gdx::write_raster(raster, cfg.output_path() / "spatial_patterns" / fmt::format("spatial_pattern_flanders_{}.tif", emissionId));
        raster = gdx::resample_raster(raster, outputGrid, gdal::ResampleAlgorithm::Average);
        normalize_raster(raster);
        raster *= emissionValue;
        return raster;
    }

    throw RuntimeError("Spatial pattern tables are only implemented for Flanders, not {}", emissionId.country);
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
    case SpatialPatternSource::Type::RasterException:
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

static void add_point_sources_to_grid(std::span<const EmissionEntry> pointEmissions, gdx::DenseRaster<double>& raster)
{
    const auto& meta = raster.metadata();

    // Add the point sources to the grid
    for (auto pointEmission : pointEmissions) {
        if (auto amount = pointEmission.value().amount(); amount.has_value()) {
            if (auto coord = pointEmission.coordinate(); coord.has_value()) {
                auto cell = meta.convert_xy_to_cell(coord->x, coord->y);
                if (meta.is_on_map(cell)) {
                    raster[cell] += *amount;
                }
            }
        }
    }
}

struct SpreadEmissionStatus
{
    // For these id's there was a spatial pattern, but it contained no infomation for this county, uniform spread was used
    std::unordered_set<EmissionIdentifier> idsWithoutSpatialPatternData;
};

SpreadEmissionStatus spread_emissions(const EmissionInventory& emissionInv, const SpatialPatternInventory& spatialPatternInv, const RunConfiguration& cfg, EmissionValidation* validator, const ModelProgress::Callback& progressCb)
{
    chrono::ScopedDurationLog d("Spread emissions");

    SpreadEmissionStatus status;

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
        std::mutex mut, statusMut;

        for (const auto& pollutant : cfg.pollutants().list()) {
            EmissionsCollector collector(cfg, pollutant, gridData);

            for (const auto& sector : cfg.sectors().nfr_sectors()) {
                ModelProgressInfo info;
                info.info = fmt::format("[{}] Spread {} emissions for sector '{}'", gridData.name, pollutant, sector.code());
                progress.set_payload(info);
                progress.tick();

                // std::for_each(countryCoverages.begin(), countryCoverages.end(), [&](const CountryCellCoverage& cellCoverageInfo) {
                tbb::parallel_for_each(countryCoverages, [&](const CountryCellCoverage& cellCoverageInfo) {
                    if (cellCoverageInfo.country == country::BEF) {
                        return;
                    }

                    /*if (cellCoverageInfo.country.iso_code() != "MD" || sector.name() != "1A4bi" || pollutant.code() != "NMVOC") {
                        return;
                    }*/

                    try {
                        EmissionIdentifier emissionId(cellCoverageInfo.country, EmissionSector(sector), pollutant);

                        const auto emission = emissionInv.try_emission_with_id(emissionId);
                        if (!emission.has_value()) {
                            Log::debug("No emissions available for pollutant {} in sector: {} in {}", pollutant, EmissionSector(sector), cellCoverageInfo.country);
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
                        auto countryRaster = apply_spatial_pattern(spatialPatternInv.get_spatial_pattern(emissionId), emissionId, emissionToSpread, cellCoverageInfo, cfg, spatPatStatus);
                        if (spatPatStatus == SpatialPatternStatus::FallbackToUniformSpread) {
                            std::scoped_lock lock(statusMut);
                            status.idsWithoutSpatialPatternData.insert(emissionId);
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

                        add_point_sources_to_grid(pointEmissions, countryRaster);

                        collector.add_diffuse_emissions(emissionId.country, sector, std::move(countryRaster));

                        if (isCoursestGrid) {
                            // Only add the point emissions once for the coursest grid as they are resolution independant
                            collector.add_point_emissions(emission->scaled_point_emissions());
                            if (validator) {
                                validator->add_point_emissions(emissionId, emission->scaled_point_emissions_sum());
                            }
                        }
                    } catch (const std::exception& e) {
                        Log::error("Error spreading emission: {}", e.what());
                    }
                });
            }

            if (gridIter + 1 == gridDefinitions.end()) {
                // Now do flanders (for finest grid)

                std::vector<SpatialPatternData> spatialPatterns;
                const auto& flandersCoverage = find_in_container_required(countryCoverages, [](const CountryCellCoverage& cov) {
                    return cov.country == country::BEF;
                });

                const auto sectors = cfg.sectors().nfr_sectors();
                // std::for_each(sectors.begin(), sectors.end(), [&](const NfrSector& sector) {
                tbb::parallel_for_each(sectors, [&](const NfrSector& sector) {
                    EmissionIdentifier emissionId(country::BEF, EmissionSector(sector), pollutant);

                    auto spatialPattern = spatialPatternInv.get_spatial_pattern(emissionId);
                    {
                        std::scoped_lock lock(mut);
                        if (spatialPatterns.empty()) {
                            if (spatialPattern.type == SpatialPatternSource::Type::SpatialPatternTable) {
                                spatialPatterns = parse_spatial_pattern_flanders(spatialPattern.path, cfg);
                            }
                        }
                    }

                    auto emission = emissionInv.emission_with_id(emissionId);

                    gdx::DenseRaster<double> raster;
                    if (spatialPattern.type == SpatialPatternSource::Type::SpatialPatternTable) {
                        const auto* spatPat = inf::find_in_container(spatialPatterns, [&](const SpatialPatternData& src) {
                            return src.id == emissionId;
                        });

                        if (spatPat != nullptr) {
                            raster = apply_spatial_pattern_flanders(spatPat->raster, emission.scaled_diffuse_emissions_sum(), gridData.meta);
                        }
                    } else if (spatialPattern.type == SpatialPatternSource::Type::RasterException) {
                        raster = apply_spatial_pattern_raster(spatialPattern.path, emission.id(), emission.scaled_diffuse_emissions_sum(), flandersCoverage);
                    }

                    if (raster.empty()) {
                        Log::debug("No spatial pattern information available for {}: falling back to uniform spread", emissionId);
                        raster = apply_uniform_spread(emission.scaled_diffuse_emissions_sum(), flandersCoverage);
                        std::scoped_lock lock(statusMut);
                        status.idsWithoutSpatialPatternData.insert(emissionId);
                    }

                    if (validator) {
                        validator->add_diffuse_emissions(emissionId, raster);
                        validator->add_point_emissions(emissionId, emission.scaled_point_emissions_sum());
                    }

                    add_point_sources_to_grid(emission.point_emissions(), raster);

                    collector.add_diffuse_emissions(emissionId.country, sector, std::move(raster));
                    collector.add_point_emissions(emission.scaled_point_emissions());
                });
            }

            collector.write_to_disk(isCoursestGrid ? EmissionsCollector::WriteMode::Create : EmissionsCollector::WriteMode::Append);
        }
    }

    return status;
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

    const auto flandersMeta   = grid_data(GridDefinition::Flanders1km).meta;
    const auto outputGridMeta = grid_data(grids_for_model_grid(cfg.model_grid()).front()).meta;

    if (outputGridMeta.projected_epsg() != flandersMeta.projected_epsg()) {
        gdal::CoordinateTransformer transformer(flandersMeta.projection, outputGridMeta.projection);

        for (auto& pointSource : result) {
            auto coord = pointSource.coordinate().value();
            transformer.transform_in_place(coord);
            pointSource.set_coordinate(coord);
        }
    }

    return result;
}

static SingleEmissions read_nfr_emissions(const RunConfiguration& cfg, RunSummary& runSummary)
{
    chrono::DurationRecorder duration;
    auto nfrTotalEmissions = parse_emissions(EmissionSector::Type::Nfr, throw_if_not_exists(cfg.total_emissions_path_nfr()), cfg);
    runSummary.add_totals_source(cfg.total_emissions_path_nfr());

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

    // Optional additional emissions that supllement or override existing emissions
    const auto extraNfrPath = cfg.total_extra_emissions_path_nfr();
    if (fs::exists(extraNfrPath)) {
        merge_emissions(nfrTotalEmissions, parse_emissions(EmissionSector::Type::Nfr, extraNfrPath, cfg));
        runSummary.add_totals_source(extraNfrPath);
    }

    Log::debug("Parse nfr emissions took: {}", duration.elapsed_time_string());

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

void run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb)
{
    tbb::global_control tbbControl(tbb::global_control::max_allowed_parallelism, cfg.max_concurrency().value_or(oneapi::tbb::info::default_concurrency()));

    RunSummary summary;

    SpatialPatternInventory spatPatInv(cfg.sectors(), cfg.pollutants(), cfg.countries(), cfg.spatial_pattern_exceptions());
    spatPatInv.scan_dir(cfg.reporting_year(), cfg.year(), cfg.spatial_pattern_path());

    clean_output_directory(cfg.output_path());

    const auto pointSourcesFlanders = read_point_sources(cfg, country::BEF, summary);
    const auto nfrTotalEmissions    = read_nfr_emissions(cfg, summary);
    const auto gnfrTotalEmissions   = read_gnfr_emissions(cfg, summary);

    ScalingFactors scalingsDiffuse;
    ScalingFactors scalingsPointSource;

    if (auto path = cfg.diffuse_scalings_path(); fs::exists(path)) {
        scalingsDiffuse = parse_scaling_factors(path, cfg);
    }

    if (auto path = cfg.point_source_scalings_path(); fs::exists(path)) {
        scalingsDiffuse = parse_scaling_factors(path, cfg);
    }

    std::unique_ptr<EmissionValidation> validator;
    if (cfg.validation_type() == ValidationType::SumValidation) {
        validator = std::make_unique<EmissionValidation>();
    }

    Log::debug("Generate emission inventory");
    chrono::DurationRecorder dur;
    const auto inventory = create_emission_inventory(nfrTotalEmissions, gnfrTotalEmissions, pointSourcesFlanders, scalingsDiffuse, scalingsPointSource, summary);
    Log::debug("Generate emission inventory took {}", dur.elapsed_time_string());

    const auto spreadStatus = spread_emissions(inventory, spatPatInv, cfg, validator.get(), progressCb);

    {
        chrono::ScopedDurationLog d("Write model run summary");
        // Create the list of spatial patterns that will be used in the model
        for (auto country : cfg.countries().list()) {
            for (auto& nfr : cfg.sectors().nfr_sectors()) {
                for (auto pol : cfg.pollutants().list()) {
                    EmissionIdentifier id(country, EmissionSector(nfr), pol);
                    const auto spatialPattern = spatPatInv.get_spatial_pattern(id);
                    if (spreadStatus.idsWithoutSpatialPatternData.count(id) > 0) {
                        summary.add_spatial_pattern_source_without_data(spatialPattern);
                    } else {
                        summary.add_spatial_pattern_source(spatialPattern);
                    }
                }
            }
        }

        if (validator) {
            summary.set_validation_results(validator->create_summary(inventory));
        }

        summary.write_summary(cfg.output_path());
    }
}
}