#include "emap/scalingfactors.h"

#include "infra/exception.h"

namespace emap {

using namespace inf;

void ScalingFactors::add_scaling_factor(ScalingFactor&& sf)
{
    _scalingFactors.push_back(std::move(sf));
}

size_t ScalingFactors::size() const noexcept
{
    return _scalingFactors.size();
}

std::optional<double> ScalingFactors::scaling_for_id(const EmissionIdentifier& id, EmissionSourceType type, date::year year) const
{
    // Go through the scaling factors list from top to bottom as defined in the excel file
    // The first match is taken
    for (auto& sf : _scalingFactors) {
        if (sf.match(id, type, year) != ScalingFactor::MatchResult::NoMatch) {
            return sf.factor();
        }
    }

    return {};
}
}