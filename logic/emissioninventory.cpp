#include "emissioninventory.h"
#include "emap/scalingfactors.h"
#include "infra/algo.h"
#include "infra/log.h"
#include "runsummary.h"

#include <cassert>
#include <numeric>

namespace emap {

using namespace inf;

EmissionSector convert_sector_to_gnfr_level(const EmissionSector& sec)
{
    return EmissionSector(sec.gnfr_sector());
}

EmissionIdentifier convert_emission_id_to_gnfr_level(const EmissionIdentifier& id)
{
    EmissionIdentifier result = id;
    result.sector             = convert_sector_to_gnfr_level(id.sector);
    return result;
}

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
            correction = *gnfrBasedTotal / nfrBasedTotal;
            summary.add_gnfr_correction(id, *gnfrBasedTotal, nfrBasedTotal, correction);
        } else {
            summary.add_gnfr_correction(id, {}, nfrBasedTotal, 1.0);
        }

        result[id] = correction;
    }

    return result;
}

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary)
{
    EmissionInventory result;

    // Create gnfr sum of the actual reported nfr values
    // Create gnfr sum of the validated gnfr values (put in map)
    // Then calculate the ratio between the two
    auto nfrCorrectionRatios = create_nfr_correction_ratios(create_nfr_sums(totalEmissionsNfr), create_gnfr_sums(totalEmissionsGnfr), runSummary);

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
            } else {
            }
        } else {
            // Rest of Europe
            if (diffuseEmission < 0.0) {
                inf::Log::warn("Negative emissions reported for {}", em.id());
                diffuseEmission = 0.0;
            }

            diffuseEmission *= find_in_map_optional(nfrCorrectionRatios, convert_emission_id_to_gnfr_level(em.id())).value_or(1.0);
        }

        EmissionInventoryEntry entry(em.id(), diffuseEmission - pointEmissionSum, std::move(pointSourceEntries));
        entry.set_diffuse_scaling(diffuseScalings.scaling_for_id(em.id()).value_or(1.0));
        entry.set_point_scaling(pointScalings.scaling_for_id(em.id()).value_or(1.0));
        result.add_emission(std::move(entry));
    }

    return result;
}

}