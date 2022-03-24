#include "emissionvalidation.h"
#include "gdx/algo/sum.h"
#include "infra/exception.h"

namespace emap {

using namespace inf;

void EmissionValidation::add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal)
{
    std::scoped_lock lock(_mutex);
    _emissionSums[id] += pointEmissionsTotal;
}

void EmissionValidation::add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster)
{
    auto sum = gdx::sum(raster);

    std::scoped_lock lock(_mutex);
    _emissionSums[id] += sum;
}

std::vector<EmissionValidation::SummaryEntry> EmissionValidation::create_summary(const EmissionInventory& emissionInv)
{
    std::vector<EmissionValidation::SummaryEntry> result;
    result.reserve(emissionInv.size());

    std::transform(emissionInv.begin(), emissionInv.end(), std::back_inserter(result), [this](const EmissionInventoryEntry& invEntry) {
        EmissionValidation::SummaryEntry summaryEntry;
        summaryEntry.id                     = invEntry.id();
        summaryEntry.emissionInventoryTotal = invEntry.scaled_total_emissions_sum();

        if (auto iter = _emissionSums.find(summaryEntry.id); iter != _emissionSums.end()) {
            summaryEntry.spreadTotal = iter->second;
        }

        return summaryEntry;
    });

    return result;
}

}