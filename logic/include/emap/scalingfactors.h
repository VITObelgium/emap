#pragma once

#include "emap/country.h"
#include "emap/emissions.h"
#include "emap/inputparsers.h"
#include "emap/pollutant.h"

#include "infra/range.h"

namespace emap {

enum class EmissionSourceType
{
    Point,
    Diffuse,
    Any,
};

class ScalingFactor
{
public:
    enum class YearMatch
    {
        Exact,    // The scaling factor applies to the exact year
        Range,    // The scaling factor applies to a year range
        WildCard, // The scaling factor applies to the wildcard '*'
        NoMatch,
    };

    ScalingFactor() = default;
    ScalingFactor(const EmissionIdentifier& id, EmissionSourceType type, date::year year, double factor)
    : ScalingFactor(id, type, {year, year}, factor)
    {
    }

    ScalingFactor(const EmissionIdentifier& id, EmissionSourceType type, inf::Range<date::year> yearRange, double factor)
    : _id(id)
    , _type(type)
    , _yearRange(yearRange)
    , _factor(factor)
    {
    }

    const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    EmissionSourceType type() const noexcept
    {
        return _type;
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

    bool id_matches(const EmissionIdentifier& id) const noexcept
    {
        if (_id.sector == sector::AnyGnfr) {
            return true;
        }

        return _id == id;
    }

    bool type_matches(EmissionSourceType type) const noexcept
    {
        if (type == EmissionSourceType::Any || _type == EmissionSourceType::Any) {
            return true;
        }

        return _type == type;
    }

    YearMatch year_match(date::year year) const noexcept
    {
        if (!_yearRange.contains(year)) {
            return YearMatch::NoMatch;
        }

        if (_yearRange.begin == _yearRange.end) {
            return YearMatch::Exact;
        }

        return _yearRange == AllYears ? YearMatch::WildCard : YearMatch::Range;
    }

private:
    EmissionIdentifier _id;
    EmissionSourceType _type = EmissionSourceType::Diffuse;
    Country _country;
    EmissionSector _sector;
    inf::Range<date::year> _yearRange;
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

    std::optional<double> point_scaling_for_id(const EmissionIdentifier& id, date::year year) const
    {
        return scaling_for_id(id, EmissionSourceType::Point, year);
    }

    std::optional<double> diffuse_scaling_for_id(const EmissionIdentifier& id, date::year year) const
    {
        return scaling_for_id(id, EmissionSourceType::Diffuse, year);
    }

private:
    std::optional<double> scaling_for_id(const EmissionIdentifier& id, EmissionSourceType type, date::year year) const;

    std::vector<ScalingFactor> _scalingFactors;
};
}
