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
    enum class MatchResult
    {
        Exact,
        Range,
        WildCard,
        NoMatch,
    };

    ScalingFactor() = default;
    ScalingFactor(std::optional<Country> country,
                  std::optional<EmissionSector> sector,
                  std::optional<Pollutant> pollutant,
                  EmissionSourceType type,
                  date::year year, double factor)
    : ScalingFactor(country, sector, pollutant, type, {year, year}, factor)
    {
    }

    ScalingFactor(std::optional<Country> country,
                  std::optional<EmissionSector> sector,
                  std::optional<Pollutant> pollutant,
                  EmissionSourceType type,
                  inf::Range<date::year> yearRange,
                  double factor)
    : _country(country)
    , _sector(sector)
    , _pollutant(pollutant)
    , _type(type)
    , _yearRange(yearRange)
    , _factor(factor)
    {
    }

    EmissionSourceType type() const noexcept
    {
        return _type;
    }

    std::optional<Country> country() const noexcept
    {
        return _country;
    }

    std::optional<Pollutant> pollutant() const noexcept
    {
        return _pollutant;
    }

    double factor() const noexcept
    {
        return _factor;
    }

    MatchResult id_match(const EmissionIdentifier& id) const noexcept
    {
        if (_pollutant.has_value() && id.pollutant != *_pollutant) {
            return MatchResult::NoMatch;
        }

        if (_sector.has_value() && id.sector != *_sector) {
            if (_sector->type() == EmissionSector::Type::Nfr || (EmissionSector(id.sector.gnfr_sector()) != *_sector)) {
                return MatchResult::NoMatch;
            }
        }

        if (_country.has_value() && id.country != *_country) {
            return MatchResult::NoMatch;
        }

        if (_pollutant.has_value() && (_sector.has_value() && _sector->type() == EmissionSector::Type::Nfr) && _country.has_value()) {
            return MatchResult::Exact;
        }

        return MatchResult::WildCard;
    }

    MatchResult type_match(EmissionSourceType type) const noexcept
    {
        if (type == EmissionSourceType::Any || _type == EmissionSourceType::Any) {
            return MatchResult::WildCard;
        }

        return _type == type ? MatchResult::Exact : MatchResult::NoMatch;
    }

    MatchResult year_match(date::year year) const noexcept
    {
        if (!_yearRange.contains(year)) {
            return MatchResult::NoMatch;
        }

        if (_yearRange.begin == _yearRange.end) {
            return MatchResult::Exact;
        }

        return _yearRange == AllYears ? MatchResult::WildCard : MatchResult::Range;
    }

    MatchResult match(const EmissionIdentifier& id, EmissionSourceType type, date::year year) const noexcept
    {
        const auto idMatch   = id_match(id);
        const auto typeMatch = type_match(type);
        const auto yearMatch = year_match(year);

        if (idMatch == MatchResult::NoMatch || typeMatch == MatchResult::NoMatch || yearMatch == MatchResult::NoMatch) {
            return MatchResult::NoMatch;
        }

        if (idMatch == MatchResult::WildCard || typeMatch == MatchResult::WildCard || yearMatch == MatchResult::WildCard || yearMatch == MatchResult::Range) {
            return MatchResult::WildCard;
        }

        return MatchResult::Exact;
    }

private:
    std::optional<Country> _country;
    std::optional<EmissionSector> _sector;
    std::optional<Pollutant> _pollutant;
    EmissionSourceType _type = EmissionSourceType::Diffuse;
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
