#include "emap/scalingfactors.h"

namespace emap {

void ScalingFactors::add_scaling_factor(ScalingFactor&& sf)
{
    _scalingFactors.push_back(std::move(sf));
}

size_t ScalingFactors::size() const noexcept
{
    return _scalingFactors.size();
}

}