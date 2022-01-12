#pragma once

#include "emap/country.h"
#include "emap/emissions.h"
#include "emap/pollutant.h"

namespace emap {

class ScalingFactor
{
public:
    ScalingFactor() = default;
    ScalingFactor(const EmissionIdentifier& id, double factor)
    : _id(id)
    , _factor(factor)
    {
    }

    const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    EmissionSector sector() const noexcept
    {
        return _id.sector;
    }

    Country country() const noexcept
    {
        return _id.country;
    }

    Pollutant pollutant() const noexcept
    {
        return _id.pollutant;
    }

    double factor() const noexcept
    {
        return _factor;
    }

private:
    EmissionIdentifier _id;
    Country _country;
    EmissionSector _sector;
    double _factor = 1.0;
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

    std::optional<double> scaling_for_id(const EmissionIdentifier& id) const noexcept
    {
        auto* scaling = inf::find_in_container(_scalingFactors, [&id](const ScalingFactor& em) {
            return em.id() == id;
        });

        std::optional<double> result;
        if (scaling != nullptr) {
            result = scaling->factor();
        }

        return result;
    }

private:
    std::vector<ScalingFactor> _scalingFactors;
};
}
