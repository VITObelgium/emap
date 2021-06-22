#pragma once

#include "emap/country.h"
#include "emap/emissions.h"
#include "emap/pollutant.h"

namespace emap {

struct ScalingFactor
{
    ScalingFactor() = default;
    ScalingFactor(Country ctry, EmissionSector sec, Pollutant pol, double fac)
    : country(ctry)
    , sector(sec)
    , pollutant(pol)
    , factor(fac)
    {
    }

    Country country = Country::Count;
    EmissionSector sector;
    Pollutant pollutant = Pollutant::Count;
    double factor       = 1.0;
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
