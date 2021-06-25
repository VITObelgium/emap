#pragma once

#include "emap/emissions.h"
#include "emap/scalingfactors.h"

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
        double diffuseEmission  = em.value().amount();
        double pointEmissionSum = 0.0;

        if (em.country().is_belgium()) {
            // For belgian regions we calculate the diffuse emissions by subtracting the point source emissions
            // from the total emissions
            const auto pointEmissions = pointSourceEmissions.emissions_with_id(em.id());
            pointEmissionSum          = std::accumulate(pointEmissions.begin(), pointEmissions.end(), 0.0, [](double total, const auto& current) {
                return total + current.value().amount();
            });
        } else {
            // Rest of Europe
            // TODO: validated results logic
        }

        EmissionInventoryEntry entry(em.id(), pointEmissionSum, diffuseEmission - pointEmissionSum);
        entry.set_diffuse_scaling(diffuseScalings.scaling_for_id(em.id()).value_or(1.0));
        entry.set_point_scaling(pointScalings.scaling_for_id(em.id()).value_or(1.0));
        result.add_emission(std::move(entry));
    }

    return result;
}

}
