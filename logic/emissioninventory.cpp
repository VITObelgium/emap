#include "emap/emissioninventory.h"
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

static EmissionInventory create_emission_inventory_impl(const SingleEmissions& totalEmissionsNfr,
                                                        const std::optional<SingleEmissions>& extraEmissions,
                                                        const SingleEmissions& pointSourceEmissions,
                                                        const ScalingFactors& diffuseScalings,
                                                        const ScalingFactors& pointScalings,
                                                        const std::unordered_map<EmissionIdentifier, double>& correctionRatios)
{
    EmissionInventory result(totalEmissionsNfr.year());
    std::vector<EmissionInventoryEntry> entries;

    for (const auto& em : totalEmissionsNfr) {
        assert(em.sector().type() == EmissionSector::Type::Nfr);

        double diffuseEmission  = em.value().amount().value_or(0.0);
        double pointEmissionSum = 0.0;
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
                if (std::abs(pointEmissionSum - diffuseEmission) < 1e-6) {
                    // Minor difference caused by rounding, make them the same
                    pointEmissionSum = diffuseEmission;
                } else {
                    throw RuntimeError("The sum of the point emissions ({}) for {} is bigger than the diffuse emissions ({}) for sector {}", pointEmissionSum, em.country(), diffuseEmission, em.sector());
                }
            }
        } else {
            // Rest of Europe
            if (diffuseEmission < 0.0) {
                inf::Log::warn("Negative emissions reported for {}", em.id());
                diffuseEmission = 0.0;
            }

            diffuseEmission *= find_in_map_optional(correctionRatios, convert_emission_id_to_gnfr_level(em.id())).value_or(1.0);
        }

        EmissionInventoryEntry entry(em.id(), diffuseEmission - pointEmissionSum, std::move(pointSourceEntries));
        entry.set_diffuse_scaling(diffuseScalings.scaling_for_id(em.id()).value_or(1.0));
        entry.set_point_scaling(pointScalings.scaling_for_id(em.id()).value_or(1.0));
        entries.push_back(entry);
    }

    result.set_emissions(std::move(entries));

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
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
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

    return create_emission_inventory_impl(totalEmissionsNfr, extraEmissions, pointSourceEmissions, diffuseScalings, pointScalings, nfrCorrectionRatios);
}

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsNfrOlder,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary)
{
    chrono::ScopedDurationLog durLog("Create emission inventory");

    const auto nfrSums      = create_nfr_sums(totalEmissionsNfr);
    const auto nfrSumsOlder = create_nfr_sums(totalEmissionsNfrOlder);
    const auto gnfrSums     = create_gnfr_sums(totalEmissionsGnfr);

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

    return create_emission_inventory(totalEmissionsNfr, extrapolatedTotalEmissionsGnfr, extraEmissions, pointSourceEmissions, diffuseScalings, pointScalings, cfg, runSummary);
}
}
