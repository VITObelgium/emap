#include "emissionvalidation.h"
#include "gdx/algo/sum.h"
#include "infra/exception.h"

namespace emap {

using namespace inf;

void EmissionValidation::add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal)
{
    std::scoped_lock lock(_mutex);
    _pointEmissionSums[id] += pointEmissionsTotal;
}

void EmissionValidation::add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster)
{
    const auto sum = gdx::sum(raster);

    std::scoped_lock lock(_mutex);
    _diffuseEmissionSums[id] += sum;
}

std::vector<EmissionValidation::SummaryEntry> EmissionValidation::create_summary(const EmissionInventory& emissionInv)
{
    std::vector<EmissionValidation::SummaryEntry> result;
    result.reserve(emissionInv.size());

    std::transform(emissionInv.begin(), emissionInv.end(), std::back_inserter(result), [this](const EmissionInventoryEntry& invEntry) {
        EmissionValidation::SummaryEntry summaryEntry;
        summaryEntry.id                       = invEntry.id();
        summaryEntry.emissionInventoryDiffuse = invEntry.scaled_diffuse_emissions_sum();
        summaryEntry.emissionInventoryPoint   = invEntry.scaled_point_emissions_sum();

        summaryEntry.spreadDiffuseTotal = find_in_map_optional(_diffuseEmissionSums, summaryEntry.id);
        summaryEntry.spreadPointTotal   = find_in_map_optional(_pointEmissionSums, summaryEntry.id);

        return summaryEntry;
    });

    return result;
}

}