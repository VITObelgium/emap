#include "emissioninventory.h"
#include "emap/scalingfactors.h"
#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/log.h"
#include "runsummary.h"

#include <cassert>
#include <numeric>

namespace emap {

using namespace inf;

static std::unordered_map<EmissionIdentifier, double> create_gnfr_sums(const SingleEmissions& totalEmissionsNfr)
{
    std::unordered_map<EmissionIdentifier, double> result;

    for (const auto& em : totalEmissionsNfr) {
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
            if (id.pollutant.code() != "TSP") {
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
        result.add_emission(std::move(entry));
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

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary)
{
    chrono::ScopedDurationLog d("Create emission inventory");

    // Create the GNFR totals of the nfr sectors
    const auto nfrSums  = create_nfr_sums(totalEmissionsNfr);
    const auto gnfrSums = create_gnfr_sums(totalEmissionsGnfr);

    // Create gnfr sum of the actual reported nfr values
    // Create gnfr sum of the validated gnfr values (put in map)
    // Then calculate the ratio between the two
    const auto nfrCorrectionRatios = create_nfr_correction_ratios(nfrSums, gnfrSums, runSummary);

    return create_emission_inventory_impl(totalEmissionsNfr, extraEmissions, pointSourceEmissions, diffuseScalings, pointScalings, nfrCorrectionRatios);
}

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsNfrOlder,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary)
{
    chrono::ScopedDurationLog d("Create emission inventory");

    const auto nfrSums      = create_nfr_sums(totalEmissionsNfr);
    const auto nfrSumsOlder = create_nfr_sums(totalEmissionsNfrOlder);
    const auto gnfrSums     = create_gnfr_sums(totalEmissionsGnfr);

    SingleEmissions extrapolatedTotalEmissionsGnfr(totalEmissionsGnfr.year() + date::years(1));
    for (auto& [id, gnfrSum] : gnfrSums) {
        auto nfrSum      = find_in_map_optional(nfrSums, id).value_or(0.0);
        auto olderNfrSum = find_in_map_optional(nfrSumsOlder, id).value_or(0.0);

        double extrapolatedGnfr = 0.0;
        if (olderNfrSum != 0.0) {
            extrapolatedGnfr = (nfrSum / olderNfrSum) * gnfrSum;
        }

        runSummary.add_gnfr_correction(id, gnfrSum, extrapolatedGnfr, nfrSum, olderNfrSum);
        extrapolatedTotalEmissionsGnfr.add_emission(EmissionEntry(id, EmissionValue(extrapolatedGnfr)));
    }

    return create_emission_inventory(totalEmissionsNfr, extrapolatedTotalEmissionsGnfr, extraEmissions, pointSourceEmissions, diffuseScalings, pointScalings, runSummary);
}

}
