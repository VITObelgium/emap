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

    constexpr const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    constexpr EmissionSector sector() const noexcept
    {
        return _id.sector;
    }

    constexpr Country country() const noexcept
    {
        return _id.country;
    }

    constexpr Pollutant pollutant() const noexcept
    {
        return _id.pollutant;
    }

    constexpr double factor() const noexcept
    {
        return _factor;
    }

private:
    EmissionIdentifier _id;
    Country _country;
    EmissionSector _sector;
    Pollutant _pollutant = Pollutant::Invalid;
    double _factor       = 1.0;
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
