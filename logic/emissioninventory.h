#pragma once

#include "emap/emissions.h"
#include "emap/scalingfactors.h"
#include "infra/log.h"

#include <numeric>

namespace emap {

using namespace inf;

inline EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissions,
                                                   const SingleEmissions& pointSourceEmissions,
                                                   const ScalingFactors& diffuseScalings,
                                                   const ScalingFactors& pointScalings)
{
    EmissionInventory result;

    for (const auto& em : totalEmissions) {
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

            // TODO: diffuseEmission > 0 check to avoid negative values causing errors
            if (diffuseEmission > 0 && pointEmissionSum > diffuseEmission) {
                throw RuntimeError("The sum of the point emissions ({}) for {} is bigger than the diffuse emissions ({}) for sector {}", pointEmissionSum, em.country(), diffuseEmission, em.sector());
            }
        } else {
            // Rest of Europe
            // TODO: validated results logic
            if (diffuseEmission < 0.0) {
                inf::Log::warn("Negative emissions reported for {}", em.id());
                diffuseEmission = 0.0;
            }
        }

        EmissionInventoryEntry entry(em.id(), diffuseEmission - pointEmissionSum, std::move(pointSourceEntries));
        entry.set_diffuse_scaling(diffuseScalings.scaling_for_id(em.id()).value_or(1.0));
        entry.set_point_scaling(pointScalings.scaling_for_id(em.id()).value_or(1.0));
        result.add_emission(std::move(entry));
    }

    return result;
}

}
