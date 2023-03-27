#include "emap/emissioninventory.h"
#include "emap/constants.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"

#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/log.h"
#include "runsummary.h"

#include <cassert>
#include <numeric>

namespace emap {

using namespace inf;
using namespace date::literals;

static fs::path throw_if_not_exists(const fs::path& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return path;
}

static std::unordered_map<EmissionIdentifier, double> create_gnfr_sums(const SingleEmissions& totalEmissionsGnfr)
{
    std::unordered_map<EmissionIdentifier, double> result;

    for (const auto& em : totalEmissionsGnfr) {
        if (em.country().is_belgium() || !em.value().amount().has_value()) {
            continue;
        }

        assert(em.sector().type() == EmissionSector::Type::Gnfr);
        assert(result.count(em.id()) == 0);

        result.emplace(em.id(), *em.value().amount());
    }

    return result;
}

static std::unordered_map<EmissionIdentifier, double> create_nfr_sums(const SingleEmissions& totalEmissionsNfr)
{
    std::unordered_map<EmissionIdentifier, double> result;

    for (const auto& em : totalEmissionsNfr) {
        if (em.country().is_belgium() || !em.value().amount().has_value()) {
            continue;
        }

        assert(em.sector().type() == EmissionSector::Type::Nfr);
        EmissionIdentifier gnfrId = convert_emission_id_to_gnfr_level(em.id());

        result[gnfrId] += *em.value().amount();
    }

    return result;
}

static SingleEmissions handle_missing_nfr_data(date::year nfrYear,
                                               const std::unordered_map<EmissionIdentifier, double>& nfrBasedTotals,
                                               const std::unordered_map<EmissionIdentifier, double>& gnfrTotals,
                                               const RunConfiguration& cfg)
{
    SingleEmissions result(nfrYear);

    for (auto& [id, sum] : nfrBasedTotals) {
        if (sum == 0.0) {
            if (auto gnfrSum = find_in_map_optional(gnfrTotals, id); gnfrSum > 0.0) {
                // Search older years for nfr data
                Log::debug("No nfr data for {}", id);
                if (!id.country.is_belgium()) {
                    bool dataFound = false;

                    // Disable for now, does not occur
                    /*while (nfrYear >= 1990_y) {
                        --nfrYear;

                        if (auto path = cfg.total_emissions_path_nfr(nfrYear); fs::is_regular_file(path)) {
                            auto emissions = parse_emissions(EmissionSector::Type::Nfr, path, nfrYear, cfg);
                            auto olderSums = create_nfr_sums(emissions);
                            if (auto olderSum = find_in_map_optional(gnfrTotals, id); olderSum > 0.0) {
                                Log::info("Data found for year: {}", static_cast<int>(nfrYear));
                                dataFound = true;
                                break;
                            }
                        }
                    }*/

                    if (!dataFound) {
                        auto nfrSectors = cfg.sectors().nfr_sectors_in_gnfr(id.sector.gnfr_sector().id());
                        if (id.sector.is_land_sector()) {
                            remove_from_container(nfrSectors, [](const NfrSector& sector) {
                                return sector.name() == "1A3bviii";
                            });
                        } else {
                            // Special sea sector handling
                            if (id.country.is_sea()) {
                                // For shipping sector the full emission is spread evenly only over the sea sectors with sea destination
                                remove_from_container(nfrSectors, [](const NfrSector& sector) {
                                    return sector.has_land_destination();
                                });
                            } else {
                                // For shipping sector the full emission is spread evenly only over the sea sectors with land destination
                                remove_from_container(nfrSectors, [](const NfrSector& sector) {
                                    return !sector.has_land_destination();
                                });
                            }
                        }

                        auto emissionPerSector = *gnfrSum / nfrSectors.size();
                        Log::info("Spread GNFR uniform over NFR sectors for {} (value = {})", id, emissionPerSector);
                        for (auto& nfr : nfrSectors) {
                            result.add_emission(EmissionEntry(EmissionIdentifier(id.country, EmissionSector(nfr), id.pollutant), EmissionValue(emissionPerSector)));
                        }
                    }
                }
            }
        }
    }

    return result;
}

static std::unordered_map<EmissionIdentifier, double> create_nfr_correction_ratios(
    const std::unordered_map<EmissionIdentifier, double>& nfrBasedTotals,
    const std::unordered_map<EmissionIdentifier, double>& gnfrBasedTotals,
    RunSummary& summary)
{
    std::unordered_map<EmissionIdentifier, double> result;

    for (auto& [id, nfrBasedTotal] : nfrBasedTotals) {
        const auto gnfrBasedTotal = inf::find_in_map_optional(gnfrBasedTotals, id);
        double correction         = 1.0;

        assert(!id.country.is_belgium());

        if (gnfrBasedTotal.has_value()) {
            if (nfrBasedTotal > 0) {
                correction = *gnfrBasedTotal / nfrBasedTotal;
            }

            summary.add_gnfr_correction(id, *gnfrBasedTotal, nfrBasedTotal, correction);
        } else {
            if (id.pollutant.code() != "TSP" &&
                id.pollutant.code() != "Zn" &&
                id.pollutant.code() != "As" &&
                id.pollutant.code() != "Ni" &&
                id.pollutant.code() != "Cu" &&
                id.pollutant.code() != "Cr" &&
                id.pollutant.code() != "Se") {
                // If no gnfr data is reported, this is a validated 0
                // so set the scale factor to 0 so the nfr is not used (except for TSP pollutant)
                correction = 0.0;
            }

            summary.add_gnfr_correction(id, {}, nfrBasedTotal, correction);
        }

        result[id] = correction;
    }

    return result;
}

// Call the callback with the matchine emissions from both arrays
template <typename Callback>
static void zip_point_emissions(std::span<const EmissionEntry> pol1Points, std::span<const EmissionEntry> pol2Points, Callback&& callback)
{
    auto pol2PointsCopy = container_as_vector(pol2Points);

    for (auto& pol1Entry : pol1Points) {
        if (pol1Entry.value().amount().has_value()) {
            auto iter = std::lower_bound(pol2PointsCopy.begin(), pol2PointsCopy.end(), pol1Entry.source_id(), [](const EmissionEntry& entry, std::string_view srcId) {
                return entry.source_id() < srcId;
            });

            if (iter != pol2PointsCopy.end() && iter->source_id() == pol1Entry.source_id()) {
                assert(iter->coordinate() == pol1Entry.coordinate());
                callback(pol1Entry, *iter);
            }
        }
    }
}

static void validate_pm10_pm25(const EmissionInventoryEntry& pm10, const EmissionInventoryEntry& pm25)
{
    const double diffThreshold = 1e-5;

    {
        // validate the point emissions, perform validation after the autoscaling, do not take user scalings into account
        const auto pm10AutoScale = pm10.point_auto_scaling_factor();
        const auto pm25AutoScale = pm25.point_auto_scaling_factor();

        const auto pm10UserScale = pm10.point_user_scaling_factor();
        const auto pm25UserScale = pm25.point_user_scaling_factor();

        zip_point_emissions(pm10.point_emissions(), pm25.point_emissions(), [diffThreshold, pm10AutoScale, pm25AutoScale, pm10UserScale, pm25UserScale](const EmissionEntry& pm10, const EmissionEntry& pm25) {
            const auto pm10AutoScaled = pm10.value().amount().value() * pm10AutoScale;
            const auto pm25AutoScaled = pm25.value().amount().value() * pm25AutoScale;
            if (pm10AutoScaled < pm25AutoScaled) {
                const auto diff = pm25AutoScaled - pm10AutoScaled;
                if (diff > diffThreshold) {
                    throw RuntimeError("Invalid PM point data for {} (PM10: {:f} (auto scale = {:f}), PM2.5 {:f} (auto scale = {:f}))", pm10.id(), *pm10.value().amount(), pm10AutoScale, *pm25.value().amount(), pm25AutoScale);
                }
            }

            const auto pm10UserScaled = pm10AutoScaled * pm10UserScale;
            const auto pm25UserScaled = pm25AutoScaled * pm25UserScale;
            if (pm10UserScaled < pm25UserScaled) {
                const auto diff = pm25UserScaled - pm10UserScaled;
                if (diff > diffThreshold) {
                    throw RuntimeError("Invalid PM point data for {} after user scaling (PM10: {:f} (auto scale = {:f} user scale = {:f}), PM2.5 {:f} (auto scale = {:f} user scale = {:f}))", pm10.id(), *pm10.value().amount(), pm10AutoScale, pm10UserScale, *pm25.value().amount(), pm25AutoScale, pm25UserScale);
                }
            }
        });
    }

    {
        // validate the diffuse emissions, do not take user scalings into account
        const auto pm10AutoScale = pm10.diffuse_auto_scaling_factor();
        const auto pm25AutoScale = pm25.diffuse_auto_scaling_factor();

        const auto pm10UserScale = pm10.diffuse_user_scaling_factor();
        const auto pm25UserScale = pm25.diffuse_user_scaling_factor();

        const auto pm10Diffuse = pm10.diffuse_emissions() * pm10AutoScale;
        const auto pm25Diffuse = pm25.diffuse_emissions() * pm25AutoScale;

        bool autoScaledValueValid = true;
        if (pm10Diffuse < pm25Diffuse) {
            if (pm25Diffuse - pm10Diffuse > diffThreshold) {
                autoScaledValueValid = false;
                Log::warn("Invalid PM diffuse data for {} (PM10: {}, PM2.5 {})", pm10.id(), pm10Diffuse, pm25Diffuse);
            }
        }

        const auto pm10UserScaled = pm10Diffuse * pm10UserScale;
        const auto pm25UserScaled = pm25Diffuse * pm25UserScale;

        if (pm10UserScaled < pm25UserScaled) {
            if (pm25UserScaled - pm10UserScaled > diffThreshold) {
                if (autoScaledValueValid) {
                    // Invalid value is caused by the user scaling
                    throw RuntimeError("Invalid PM diffuse data after user scaling for {} (PM10: {} (auto scale = {} user scale = {}), PM2.5 {} (auto scale = {} user scale = {}))", pm10.id(), pm10Diffuse, pm10AutoScale, pm10UserScale, pm25Diffuse, pm25AutoScale, pm25UserScale);
                }
            }
        }
    }
}

static void calculate_pmcoarse_emissions(const RunConfiguration& cfg, EmissionInventory& inv)
{
    // Check if pm2.5 > pm10 after scaling, if so set pmcoarse to 0
    auto pm10Pol     = cfg.pollutants().try_pollutant_from_string(constants::pollutant::PM10);
    auto pm25Pol     = cfg.pollutants().try_pollutant_from_string(constants::pollutant::PM2_5);
    auto pmCoarsePol = cfg.pollutants().try_pollutant_from_string(constants::pollutant::PMCoarse);

    if (pm10Pol.has_value() && pm25Pol.has_value() && pmCoarsePol.has_value()) {
        for (auto country : cfg.countries().list()) {
            for (auto sector : cfg.sectors().nfr_sectors()) {
                EmissionIdentifier pm10Id(country, EmissionSector(sector), *pm10Pol);
                EmissionIdentifier pm25Id(country, EmissionSector(sector), *pm25Pol);

                auto pm10Entry = inv.try_emission_with_id(pm10Id);
                auto pm25Entry = inv.try_emission_with_id(pm25Id);

                if (pm25Entry.has_value() && pm10Entry.has_value()) {
                    EmissionIdentifier pmCoarseId(country, EmissionSector(sector), *pmCoarsePol);

                    // Verify pm10 is larger then pm2.5
                    validate_pm10_pm25(*pm10Entry, *pm25Entry);

                    std::vector<EmissionEntry> pmCoarsePoints;

                    // Calculate the PMCoarse point sources
                    auto pm10AutoScale = pm10Entry->point_auto_scaling_factor();
                    auto pm25AutoScale = pm25Entry->point_auto_scaling_factor();
                    auto pm10UserScale = pm10Entry->point_user_scaling_factor();
                    auto pm25UserScale = pm25Entry->point_user_scaling_factor();
                    zip_point_emissions(pm10Entry->point_emissions(), pm25Entry->point_emissions(), [=, &pmCoarsePoints](const EmissionEntry& pm10, const EmissionEntry& pm25) {
                        try {
                            auto pm10Scaled = *pm10.value().amount() * pm10AutoScale * pm10UserScale;
                            auto pm25Scaled = *pm25.value().amount() * pm25AutoScale * pm25UserScale;

                            EmissionEntry entry(EmissionIdentifier(country, pm10.id().sector, *pmCoarsePol), EmissionValue(pm10Scaled - pm25Scaled));
                            entry.set_coordinate(pm10.coordinate().value());
                            pmCoarsePoints.push_back(std::move(entry));
                        } catch (const std::exception& e) {
                            throw RuntimeError("Sector {} with EIL nr {} ({})", pm10.id().sector, pm10.source_id(), e.what());
                        }
                    });

                    // Calculate the PMCoarse diffuse sources
                    const auto pm25Sum     = pm25Entry->scaled_diffuse_emissions_sum();
                    const auto pm10Sum     = pm10Entry->scaled_diffuse_emissions_sum();
                    double pmCoarseDiffuse = pm10Sum - pm25Sum;
                    if (pmCoarseDiffuse < -1e-5) {
                        pmCoarseDiffuse = 0.0;
                        Log::debug("{} {} PM2.5 value is bigger then PM10 value after scaling: PM2.5={} PM10={}", country.iso_code(), sector.name(), pm25Sum, pm10Sum);
                    }

                    inv.add_emission(EmissionInventoryEntry(pmCoarseId, pmCoarseDiffuse, pmCoarsePoints));
                }
            }
        }
    }
}

static EmissionInventory create_emission_inventory_impl(const SingleEmissions& totalEmissionsNfr,
                                                        const std::optional<SingleEmissions>& extraEmissions,
                                                        const SingleEmissions& pointSourceEmissions,
                                                        const ScalingFactors& scalings,
                                                        const std::unordered_map<EmissionIdentifier, double>& correctionRatios,
                                                        const RunConfiguration& cfg)
{
    EmissionInventory result(totalEmissionsNfr.year());
    std::vector<EmissionInventoryEntry> entries;

    for (const auto& em : totalEmissionsNfr) {
        assert(em.sector().type() == EmissionSector::Type::Nfr);

        double diffuseEmission          = em.value().amount().value_or(0.0);
        double diffuseEmissionAutoScale = 1.0;
        double pointEmissionSum         = 0.0;
        double pointEmissionAutoScale   = 1.0;
        std::vector<EmissionEntry> pointSourceEntries;

        if (em.country().is_belgium()) {
            // For belgian regions we calculate the diffuse emissions by subtracting the point source emissions
            // from the total emissions

            pointSourceEntries = pointSourceEmissions.emissions_with_id(em.id());
            pointEmissionSum   = std::accumulate(pointSourceEntries.cbegin(), pointSourceEntries.cend(), 0.0, [](double total, const auto& current) {
                return total + current.value().amount().value_or(0.0);
            });

            if (diffuseEmission > 0 && pointEmissionSum > diffuseEmission) {
                // Check if the difference is caused by floating point rounding
                auto scalingFactor = diffuseEmission / pointEmissionSum;
                if (pointEmissionSum - diffuseEmission < 1e-4) {
                    // Minor difference caused by rounding, make them the same
                    pointEmissionSum = diffuseEmission;
                } else if (scalingFactor * 100 >= cfg.point_source_rescale_threshold()) {
                    pointEmissionAutoScale = scalingFactor;
                    Log::info("The sum of the point emissions ({}) for {} is bigger than the total emissions ({}) for sector {} and pollutant {} but is within the rescale threshold {} >= {}", pointEmissionSum,
                              em.country(),
                              diffuseEmission,
                              em.sector(),
                              em.pollutant(),
                              scalingFactor * 100,
                              cfg.point_source_rescale_threshold());
                } else {
                    throw RuntimeError("The sum of the point emissions ({}) for {} is bigger than the total emissions ({}) for sector {} and pollutant {} and fails the rescale threshold {} < {}",
                                       pointEmissionSum,
                                       em.country(),
                                       diffuseEmission,
                                       em.sector(),
                                       em.pollutant(),
                                       scalingFactor * 100,
                                       cfg.point_source_rescale_threshold());
                }
            }
        } else {
            // Rest of Europe
            if (diffuseEmission < 0.0) {
                inf::Log::warn("Negative emissions reported for {}", em.id());
                diffuseEmission = 0.0;
            }

            diffuseEmissionAutoScale = find_in_map_optional(correctionRatios, convert_emission_id_to_gnfr_level(em.id())).value_or(1.0);
        }

        diffuseEmission = std::max(0.0, diffuseEmission - (pointEmissionSum * pointEmissionAutoScale));

        EmissionInventoryEntry entry(em.id(), diffuseEmission, std::move(pointSourceEntries));
        entry.set_diffuse_auto_scaling(diffuseEmissionAutoScale);
        entry.set_diffuse_user_scaling(scalings.diffuse_scaling_for_id(em.id(), result.year()).value_or(1.0));
        entry.set_point_auto_scaling(pointEmissionAutoScale);
        entry.set_point_user_scaling(scalings.point_scaling_for_id(em.id(), result.year()).value_or(1.0));
        entries.push_back(entry);
    }

    result.set_emissions(std::move(entries));

    if (cfg.pmcoarse_calculation_needed()) {
        calculate_pmcoarse_emissions(cfg, result);
    }

    if (extraEmissions.has_value()) {
        for (const auto& em : *extraEmissions) {
            if (em.sector().type() != EmissionSector::Type::Nfr) {
                throw RuntimeError("Additional emission should be for NFR sectors");
            }

            if (em.value().amount().has_value()) {
                result.update_or_add_emission(EmissionInventoryEntry(em.id(), *em.value().amount()));
            }
        }
    }

    return result;
}

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& scalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary)
{
    chrono::ScopedDurationLog d("Create emission inventory");

    // Create the GNFR totals of the nfr sectors
    auto nfrSums        = create_nfr_sums(totalEmissionsNfr);
    const auto gnfrSums = create_gnfr_sums(totalEmissionsGnfr);

    for (auto& [id, sum] : gnfrSums) {
        if (nfrSums.count(id) == 0) {
            // No nfr emission available, set it to 0
            nfrSums[id] = 0;
        }
    }

    // Create gnfr sum of the actual reported nfr values
    // Create gnfr sum of the validated gnfr values (put in map)
    // Then calculate the ratio between the two
    const auto nfrCorrectionRatios = create_nfr_correction_ratios(nfrSums, gnfrSums, runSummary);

    // Add missing nfr data to the nfr emissions, don't use merge_unique_emissions, we need to overwrite existing zero entries
    merge_emissions(totalEmissionsNfr, handle_missing_nfr_data(totalEmissionsNfr.year(), nfrSums, gnfrSums, cfg));

    return create_emission_inventory_impl(totalEmissionsNfr, extraEmissions, pointSourceEmissions, scalings, nfrCorrectionRatios, cfg);
}

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsNfrOlder,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& scalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary)
{
    chrono::ScopedDurationLog durLog("Create emission inventory");

    const auto nfrSums      = create_nfr_sums(totalEmissionsNfr);      //  NFR: Y-2
    const auto nfrSumsOlder = create_nfr_sums(totalEmissionsNfrOlder); //  NFR: Y-3
    const auto gnfrSums     = create_gnfr_sums(totalEmissionsGnfr);    // GNFR: Y-2

    // Extrapolate the emissions based on the previous GNFR emissions
    SingleEmissions extrapolatedTotalEmissionsGnfr(totalEmissionsGnfr.year() + date::years(1));

    std::unordered_map<EmissionIdentifier, double> correctedGnfrSums;

    for (auto& [id, gnfrSum] : gnfrSums) {
        auto nfrSum = find_in_map_optional(nfrSums, id);
        if (!nfrSum.has_value()) {
            continue;
        }

        auto olderNfrSum = find_in_map_optional(nfrSumsOlder, id).value_or(0.0);

        double extrapolatedGnfr = gnfrSum;
        if (olderNfrSum != 0.0) {
            extrapolatedGnfr = (*nfrSum / olderNfrSum) * gnfrSum;
        }

        runSummary.add_gnfr_correction(id, gnfrSum, extrapolatedGnfr, *nfrSum, olderNfrSum);
        extrapolatedTotalEmissionsGnfr.add_emission(EmissionEntry(id, EmissionValue(extrapolatedGnfr)));
        correctedGnfrSums[id] = extrapolatedGnfr;
    }

    return create_emission_inventory(totalEmissionsNfr, extrapolatedTotalEmissionsGnfr, extraEmissions, pointSourceEmissions, scalings, cfg, runSummary);
}

static SingleEmissions read_country_pollutant_point_sources(const fs::path& dir, const Pollutant& pol, const RunConfiguration& cfg, RunSummary& runSummary)
{
    // first check if emap_{scenario}_{pol}_{year}_ exists
    // otherwise use emap_{pol}_{year}_

    auto scenarioMatch = std::string();
    auto match         = fmt::format("emap_{}_{}_", pol.code(), static_cast<int>(cfg.year()));
    if (auto scenario = cfg.scenario(); !scenario.empty()) {
        scenarioMatch = fmt::format("emap_{}_{}_{}_", scenario, pol.code(), static_cast<int>(cfg.year()));
    }

    std::set<fs::path> pathsToUse;

    auto insertMatchingFiles = [&pathsToUse](std::string_view match, const fs::path& dir) {
        for (auto iter : fs::directory_iterator(dir)) {
            if (iter.is_regular_file() && iter.path().extension() == ".csv") {
                const auto& path = iter.path();
                if (str::starts_with(path.filename().u8string(), match)) {
                    pathsToUse.insert(iter.path());
                }
            }
        }
    };

    if (!scenarioMatch.empty()) {
        // Find scenario specific matches
        insertMatchingFiles(scenarioMatch, dir);
    }

    if (pathsToUse.empty()) {
        insertMatchingFiles(match, dir);
    }

    SingleEmissions result(cfg.year());
    for (auto& path : pathsToUse) {
        merge_unique_emissions(result, parse_point_sources(path, cfg));
        runSummary.add_point_source(path);
    }

    return result;
}

SingleEmissions read_country_point_sources(const RunConfiguration& cfg, const Country& country, RunSummary& runSummary)
{
    SingleEmissions result(cfg.year());

    if (country == country::BEF) {
        auto pointEmissionsDir = cfg.point_source_emissions_dir_path(country);

        for (const auto& pollutant : cfg.included_pollutants()) {
            if (pollutant.code() != constants::pollutant::PMCoarse) {
                merge_unique_emissions(result, read_country_pollutant_point_sources(pointEmissionsDir, pollutant, cfg, runSummary));
            }
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
    }

    return result;
}

SingleEmissions read_nfr_emissions(date::year year, const RunConfiguration& cfg, RunSummary& runSummary)
{
    chrono::DurationRecorder duration;

    date::year reportYear = cfg.reporting_year();
    fs::path totalEmissionsNfrPath;
    while (totalEmissionsNfrPath.empty()) {
        if (auto path = cfg.total_emissions_path_nfr(year, reportYear); fs::is_regular_file(path)) {
            totalEmissionsNfrPath = path;
        } else {
            --reportYear;
        }

        if (cfg.reporting_year() - reportYear > date::years(10)) {
            throw RuntimeError("NFR emissions could not be found");
        }
    }

    auto nfrTotalEmissions = parse_emissions(EmissionSector::Type::Nfr, throw_if_not_exists(totalEmissionsNfrPath), year, cfg, RespectIgnoreList::Yes);
    runSummary.add_totals_source(totalEmissionsNfrPath);

    static const std::array<const Country*, 3> belgianRegions = {
        &country::BEB,
        &country::BEF,
        &country::BEW,
    };

    for (auto* region : belgianRegions) {
        const auto& path = cfg.total_emissions_path_nfr_belgium(*region);
        merge_unique_emissions(nfrTotalEmissions, parse_emissions_belgium(path, year, cfg));
        runSummary.add_totals_source(path);
    }

    Log::debug("Parse nfr emissions took: {}", duration.elapsed_time_string());

    return nfrTotalEmissions;
}

static SingleEmissions read_gnfr_emissions(const RunConfiguration& cfg, RunSummary& runSummary, date::year& reportYear)
{
    chrono::DurationRecorder duration;

    std::optional<SingleEmissions> gnfrTotalEmissions;

    auto reportedGnfrDataPath = cfg.total_emissions_path_gnfr(cfg.reporting_year());
    if (fs::is_regular_file(reportedGnfrDataPath)) {
        // Validated gnfr data is available
        reportYear         = cfg.reporting_year();
        gnfrTotalEmissions = parse_emissions(EmissionSector::Type::Gnfr, reportedGnfrDataPath, cfg.year(), cfg, RespectIgnoreList::Yes);
    }

    if (!gnfrTotalEmissions.has_value() || gnfrTotalEmissions->empty()) {
        // No GNFR data available yet for this year, read last years Gnfr data
        auto year = cfg.year();
        if (year > cfg.reporting_year() - date::years(2)) {
            throw RuntimeError("The requested year is too recent should be {} or earlier", static_cast<int>(cfg.reporting_year() - date::years(2)));
        } else if (year == cfg.reporting_year() - date::years(2)) {
            --year; // Read GNFR Y-3
        }

        reportYear           = cfg.reporting_year() - date::years(1);
        reportedGnfrDataPath = cfg.total_emissions_path_gnfr(reportYear);
        gnfrTotalEmissions   = parse_emissions(EmissionSector::Type::Gnfr, throw_if_not_exists(reportedGnfrDataPath), year, cfg, RespectIgnoreList::Yes);
        if (gnfrTotalEmissions->empty()) {
            throw RuntimeError("No GNFR data could be found for the requested year, nor for the previous year");
        }
    }

    runSummary.add_totals_source(reportedGnfrDataPath);
    Log::debug("Parse gnfr emissions took: {}", duration.elapsed_time_string());
    return *gnfrTotalEmissions;
}

static ScalingFactors read_scaling_factors(const fs::path& p, const RunConfiguration& cfg)
{
    ScalingFactors scalings;

    if (!p.empty() && fs::is_regular_file(p)) {
        scalings = parse_scaling_factors(p, cfg);
    }

    return scalings;
}

EmissionInventory make_emission_inventory(const RunConfiguration& cfg, RunSummary& summary)
{
    // Read scaling factors
    const auto scalings             = read_scaling_factors(cfg.emission_scalings_path(), cfg);
    const auto pointSourcesFlanders = read_country_point_sources(cfg, country::BEF, summary);
    auto nfrTotalEmissions          = read_nfr_emissions(cfg.year(), cfg, summary);
    assert(nfrTotalEmissions.validate_uniqueness());

    // Optional additional emissions that suplement or override existing emissions
    std::optional<SingleEmissions> extraEmissions;
    const auto extraNfrPath = cfg.total_extra_emissions_path_nfr();
    if (fs::exists(extraNfrPath)) {
        extraEmissions = parse_emissions(EmissionSector::Type::Nfr, extraNfrPath, cfg.year(), cfg, RespectIgnoreList::No);
        summary.add_totals_source(extraNfrPath);
    }

    date::year gnfrReportYear;
    auto gnfrTotalEmissions = read_gnfr_emissions(cfg, summary, gnfrReportYear);
    assert(gnfrTotalEmissions.validate_uniqueness());

    if (gnfrReportYear < cfg.reporting_year() && (cfg.year() == (cfg.reporting_year() - date::years(2)))) {
        // no gnfr data was available for the reporting year, older data was read
        // and interpolation is needed for recent years: year = report_year - 2
        auto olderNfrTotalEmissions = read_nfr_emissions(cfg.year() - date::years(1), cfg, summary);
        return create_emission_inventory(std::move(nfrTotalEmissions),
                                         std::move(olderNfrTotalEmissions),
                                         std::move(gnfrTotalEmissions),
                                         extraEmissions,
                                         pointSourcesFlanders,
                                         scalings,
                                         cfg,
                                         summary);
    } else {
        return create_emission_inventory(std::move(nfrTotalEmissions),
                                         std::move(gnfrTotalEmissions),
                                         extraEmissions,
                                         pointSourcesFlanders,
                                         scalings,
                                         cfg,
                                         summary);
    }
}
}
