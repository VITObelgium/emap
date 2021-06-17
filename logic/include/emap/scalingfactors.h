#pragma once

#include "emap/emissions.h"
#include <string_view>

namespace emap {

struct ScalingFactor
{
    ScalingFactor() = default;
    ScalingFactor(std::string_view ctry, EmissionSector sec, std::string_view pol, double fac)
    : country(ctry)
    , sector(sec)
    , pollutant(pol)
    , factor(fac)
    {
    }

    std::string country;
    EmissionSector sector;
    std::string pollutant;
    double factor = 1.0;
};

class ScalingFactors
{
public:
    void add_scaling_factor(ScalingFactor&& sf);
    size_t size() const noexcept;

    auto begin() const noexcept
    {
        return _scalingFactors.begin();
    }

    auto end() const noexcept
    {
        return _scalingFactors.end();
    }

private:
    std::vector<ScalingFactor> _scalingFactors;
};
}