#pragma once

#include "emap/country.h"
#include "emap/pollutant.h"
#include "emap/sector.h"
#include "infra/algo.h"
#include "infra/point.h"

#include <date/date.h>
#include <fmt/core.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emap {

using Coordinate = inf::Point<int32_t>;

enum class EmissionType
{
    Historic,
    Future,
};

class EmissionValue
{
public:
    constexpr EmissionValue() noexcept = default;
    constexpr explicit EmissionValue(double amount)
    : _amount(amount)
    {
    }

    constexpr double amount() const noexcept
    {
        return _amount;
    }

    constexpr std::string_view unit() const noexcept
    {
        return "Gg";
    }

    constexpr EmissionValue operator+(const EmissionValue& other) const noexcept
    {
        return EmissionValue(_amount + other._amount);
    }

    constexpr EmissionValue& operator+=(const EmissionValue& other) noexcept
    {
        _amount += other._amount;
        return *this;
    }

private:
    double _amount = 0.0;
};

struct EmissionIdentifier
{
    constexpr EmissionIdentifier() noexcept = default;
    constexpr EmissionIdentifier(Country country_, EmissionSector sector_, Pollutant pollutant_) noexcept
    : country(country_)
    , sector(sector_)
    , pollutant(pollutant_)
    {
    }

    constexpr bool operator==(const EmissionIdentifier& other) const noexcept
    {
        return country == other.country && sector == other.sector && pollutant == other.pollutant;
    }

    constexpr bool operator!=(const EmissionIdentifier& other) const noexcept
    {
        return !(*this == other);
    }

    Country country = Country::Id::Invalid;
    EmissionSector sector;
    Pollutant pollutant = Pollutant::Invalid;
};

class EmissionEntry
{
public:
    constexpr EmissionEntry() noexcept = default;
    constexpr EmissionEntry(EmissionIdentifier id, EmissionValue value) noexcept
    : EmissionEntry(id, value, {})
    {
    }

    constexpr EmissionEntry(EmissionIdentifier id, EmissionValue value, Coordinate coordinate) noexcept
    : _id(id)
    , _value(value)
    , _coordinate(coordinate)
    {
    }

    constexpr const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    constexpr void set_coordinate(Coordinate coordinate) noexcept
    {
        _coordinate = std::optional<Coordinate>(coordinate);
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

    constexpr const EmissionValue& value() const noexcept
    {
        return _value;
    }

    constexpr std::optional<Coordinate> coordinate() const noexcept
    {
        return _coordinate;
    }

private:
    EmissionIdentifier _id;
    EmissionValue _value;
    std::optional<Coordinate> _coordinate;
};

class EmissionInventoryEntry
{
public:
    constexpr EmissionInventoryEntry() noexcept = default;
    constexpr EmissionInventoryEntry(EmissionIdentifier id, double pointEmissions, double diffuseEmissions) noexcept
    : _id(id)
    , _pointEmission(pointEmissions)
    , _diffuseEmission(diffuseEmissions)
    {
    }

    constexpr const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    constexpr double total_emissions() const noexcept
    {
        return _pointEmission + _diffuseEmission;
    }

    constexpr double diffuse_emissions() const noexcept
    {
        return _diffuseEmission;
    }

    constexpr double point_emissions() const noexcept
    {
        return _pointEmission;
    }

    constexpr double scaled_total_emissions() const noexcept
    {
        return scaled_point_emissions() + scaled_diffuse_emissions();
    }

    constexpr double scaled_diffuse_emissions() const noexcept
    {
        return _diffuseEmission * _diffuseScaling;
    }

    constexpr double scaled_point_emissions() const noexcept
    {
        return _pointEmission * _pointScaling;
    }

    constexpr void set_point_scaling(double factor) noexcept
    {
        _pointScaling = factor;
    }

    constexpr void set_diffuse_scaling(double factor) noexcept
    {
        _diffuseScaling = factor;
    }

private:
    EmissionIdentifier _id;
    double _pointEmission   = 0.0;
    double _diffuseEmission = 0.0;
    double _pointScaling    = 1.0;
    double _diffuseScaling  = 1.0;
};

template <typename TEmission>
class EmissionCollection
{
public:
    void add_emission(TEmission&& info)
    {
        _emissions.push_back(std::move(info));
    }

    const TEmission& emission_with_id(const EmissionIdentifier& id) const
    {
        return inf::find_in_container_required(_emissions, [&id](const TEmission& em) {
            return em.id() == id;
        });
    }

    TEmission& emission_with_id(const EmissionIdentifier& id)
    {
        return inf::find_in_container_required(_emissions, [&id](const TEmission& em) {
            return em.id() == id;
        });
    }

    std::vector<TEmission> emissions_with_id(const EmissionIdentifier& id) const
    {
        std::vector<TEmission> result;
        std::copy_if(_emissions.begin(), _emissions.end(), std::back_inserter(result), [&id](const TEmission& em) {
            return em.id() == id;
        });

        return result;
    }

    size_t size() const noexcept
    {
        return _emissions.size();
    }

    auto begin() const noexcept
    {
        return _emissions.begin();
    }

    auto end() const noexcept
    {
        return _emissions.end();
    }

    auto begin() noexcept
    {
        return _emissions.begin();
    }

    auto end() noexcept
    {
        return _emissions.end();
    }

private:
    std::vector<TEmission> _emissions;
};

using SingleEmissions   = EmissionCollection<EmissionEntry>;
using EmissionInventory = EmissionCollection<EmissionInventoryEntry>;

std::string_view emission_type_name(EmissionType type);
std::string_view emission_sector_type_name(EmissionSector::Type type);

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
