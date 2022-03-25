#pragma once

#include "emap/country.h"
#include "emap/pollutant.h"
#include "emap/sector.h"
#include "infra/algo.h"
#include "infra/exception.h"
#include "infra/hash.h"
#include "infra/point.h"
#include "infra/span.h"

#include <date/date.h>
#include <fmt/core.h>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emap {

using Coordinate = inf::Point<double>;

enum class EmissionType
{
    Historic,
    Future,
};

class EmissionValue
{
public:
    EmissionValue() noexcept = default;
    explicit EmissionValue(std::optional<double> amount)
    : _amount(amount)
    {
    }

    std::optional<double> amount() const noexcept
    {
        return _amount;
    }

    std::string_view unit() const noexcept
    {
        return "Gg";
    }

    EmissionValue operator+(const EmissionValue& other) const noexcept
    {
        return EmissionValue(_amount.value_or(0.0) + other._amount.value_or(0.0));
    }

    EmissionValue operator*(double val) const noexcept
    {
        return EmissionValue(_amount.value_or(0.0) * val);
    }

    EmissionValue& operator+=(const EmissionValue& other) noexcept
    {
        if (_amount.has_value()) {
            *_amount += other._amount.value_or(0.0);
        } else {
            _amount = other._amount;
        }

        return *this;
    }

private:
    std::optional<double> _amount;
};

struct EmissionIdentifier
{
    EmissionIdentifier() noexcept = default;
    EmissionIdentifier(Country country_, EmissionSector sector_, Pollutant pollutant_) noexcept
    : country(country_)
    , sector(sector_)
    , pollutant(pollutant_)
    {
    }

    bool operator==(const EmissionIdentifier& other) const noexcept
    {
        return country == other.country && sector == other.sector && pollutant == other.pollutant;
    }

    bool operator!=(const EmissionIdentifier& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(const EmissionIdentifier& other) const noexcept
    {
        if (country.id() < other.country.id()) {
            return true;
        } else if (country.id() == other.country.id()) {
            if (sector.id() < other.sector.id()) {
                return true;
            } else if (sector.id() == other.sector.id()) {
                return pollutant.code() < other.pollutant.code();
            }
        }

        return false;
    }

    EmissionIdentifier with_pollutant(const Pollutant& pol) const noexcept
    {
        return EmissionIdentifier(country, sector, pol);
    }

    Country country;
    EmissionSector sector;
    Pollutant pollutant;
};

class EmissionEntry
{
public:
    EmissionEntry() noexcept = default;
    EmissionEntry(EmissionIdentifier id, EmissionValue value) noexcept
    : EmissionEntry(id, value, {})
    {
    }

    EmissionEntry(EmissionIdentifier id, EmissionValue value, Coordinate coordinate) noexcept
    : _id(id)
    , _value(value)
    , _coordinate(coordinate)
    {
    }

    const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    void set_coordinate(Coordinate coordinate) noexcept
    {
        _coordinate = std::optional<Coordinate>(coordinate);
    }

    void set_source_id(std::string_view srcId) noexcept
    {
        _sourceId = srcId;
    }

    std::string_view source_id() const noexcept
    {
        return _sourceId;
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

    const EmissionValue& value() const noexcept
    {
        return _value;
    }

    void set_value(EmissionValue value) noexcept
    {
        _value = value;
    }

    std::optional<Coordinate> coordinate() const noexcept
    {
        return _coordinate;
    }

    double height() const noexcept
    {
        return _height;
    }

    double diameter() const noexcept
    {
        return _diameter;
    }

    double temperature() const noexcept
    {
        return _temperature;
    }

    double warmth_contents() const noexcept
    {
        return _warmthContents;
    }

    double flow_rate() const noexcept
    {
        return _flowRate;
    }

    void set_height(double val) noexcept
    {
        _height = val;
    }

    void set_diameter(double val) noexcept
    {
        _diameter = val;
    }

    void set_temperature(double val) noexcept
    {
        _temperature = val;
    }

    void set_warmth_contents(double val) noexcept
    {
        _warmthContents = val;
    }

    void set_flow_rate(double val) noexcept
    {
        _flowRate = val;
    }

private:
    EmissionIdentifier _id;
    EmissionValue _value;
    std::optional<Coordinate> _coordinate;
    double _height         = 0.0;
    double _diameter       = 0.0;
    double _temperature    = 0.0;
    double _warmthContents = 0.0;
    double _flowRate       = 0.0;
    std::string _sourceId; // optional source identifier
};

std::string_view emission_type_name(EmissionType type);
std::string_view emission_sector_type_name(EmissionSector::Type type);

inline EmissionSector convert_sector_to_gnfr_level(const EmissionSector& sec)
{
    return EmissionSector(sec.gnfr_sector());
}

inline EmissionIdentifier convert_emission_id_to_gnfr_level(const EmissionIdentifier& id)
{
    EmissionIdentifier result = id;
    result.sector             = convert_sector_to_gnfr_level(id.sector);
    return result;
}

}

namespace fmt {
template <>
struct formatter<emap::EmissionIdentifier>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::EmissionIdentifier& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{} - {} - {}", val.country, val.sector, val.pollutant);
    }
};

template <>
struct formatter<emap::EmissionType>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::EmissionType& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), emission_type_name(val));
    }
};

template <>
struct formatter<emap::EmissionSector::Type>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::EmissionSector::Type& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), emission_sector_type_name(val));
    }
};
}

namespace std {
template <>
struct hash<emap::EmissionIdentifier>
{
    size_t operator()(const emap::EmissionIdentifier& id) const
    {
        size_t seed = 0;
        inf::hash_combine(seed, id.country.id(), id.pollutant.code(), id.sector.id());
        return seed;
    }
};
}
