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

    for (const auto& invEntry : emissionInv) {
        if (invEntry.id().sector.type() == EmissionSector::Type::Gnfr) {
            continue;
        }

        EmissionValidation::SummaryEntry summaryEntry;
        summaryEntry.id                     = invEntry.id();
        summaryEntry.emissionInventoryTotal = invEntry.scaled_total_emissions_sum();

        if (auto iter = _emissionSums.find(summaryEntry.id); iter != _emissionSums.end()) {
            summaryEntry.spreadTotal = iter->second;
        }

        if (auto gnfrEntry = emissionInv.try_emission_with_id(convert_emission_id_to_gnfr_level(invEntry.id())); gnfrEntry.has_value()) {
            summaryEntry.gnfrTotal = gnfrEntry->diffuse_emissions();
        }

        result.push_back(summaryEntry);
    }

    return result;
}

}