#pragma once

#include "emap/country.h"
#include "emap/pollutant.h"
#include "emap/sector.h"
#include "infra/algo.h"
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

using Coordinate = inf::Point<int32_t>;

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

class EmissionInventoryEntry
{
public:
    EmissionInventoryEntry() noexcept = default;
    EmissionInventoryEntry(EmissionIdentifier id, double diffuseEmissions) noexcept
    : _id(id)
    , _diffuseEmission(diffuseEmissions)
    {
    }

    EmissionInventoryEntry(EmissionIdentifier id, double diffuseEmissions, std::vector<EmissionEntry> pointEmissionEntries) noexcept
    : _id(id)
    , _diffuseEmission(diffuseEmissions)
    , _pointEmissionEntries(std::move(pointEmissionEntries))
    {
    }

    const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    double total_emissions() const noexcept
    {
        return point_emission_sum() + _diffuseEmission;
    }

    double diffuse_emissions() const noexcept
    {
        return _diffuseEmission;
    }

    double point_emission_sum() const noexcept
    {
        return std::accumulate(_pointEmissionEntries.cbegin(), _pointEmissionEntries.cend(), 0.0, [](double total, const auto& current) {
            return total + current.value().amount().value_or(0.0);
        });
    }

    std::span<const EmissionEntry> point_emissions() const noexcept
    {
        return _pointEmissionEntries;
    }

    double scaled_total_emissions() const noexcept
    {
        return scaled_point_emissions() + scaled_diffuse_emissions();
    }

    double scaled_diffuse_emissions() const noexcept
    {
        return _diffuseEmission * _diffuseScaling;
    }

    double scaled_point_emissions() const noexcept
    {
        return point_emission_sum() * _pointScaling;
    }

    void set_point_scaling(double factor) noexcept
    {
        _pointScaling = factor;
    }

    void set_diffuse_scaling(double factor) noexcept
    {
        _diffuseScaling = factor;
    }

private:
    EmissionIdentifier _id;
    double _diffuseEmission = 0.0;
    std::vector<EmissionEntry> _pointEmissionEntries;
    double _pointScaling   = 1.0;
    double _diffuseScaling = 1.0;
};

template <typename TEmission>
class EmissionCollection
{
public:
    void add_emission(TEmission&& info)
    {
        _emissions.push_back(std::move(info));
    }

    void update_or_add_emission(TEmission&& info)
    {
        auto* emission = inf::find_in_container(_emissions, [&info](const TEmission& em) {
            return em.id() == info.id();
        });

        if (emission) {
            *emission = info;
        } else {
            add_emission(std::forward<TEmission&&>(info));
        }
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

    std::vector<const TEmission*> emissions_for_pollutant(Pollutant pol) const
    {
        std::vector<const TEmission*> result;
        for (auto& emission : _emissions) {
            if (emission.id().pollutant == pol) {
                result.push_back(&emission);
            }
        }

        return result;
    }

    std::vector<const TEmission*> emissions_for_pollutant_for_sector(Pollutant pol, EmissionSector sector) const
    {
        std::vector<const TEmission*> result;
        for (auto& emission : _emissions) {
            if (emission.id().pollutant == pol && emission.id().sector == sector) {
                result.push_back(&emission);
            }
        }

        return result;
    }

    size_t empty() const noexcept
    {
        return _emissions.empty();
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

template <typename T>
void merge_emissions(EmissionCollection<T>& output, EmissionCollection<T>&& toMerge)
{
    if (output.empty()) {
        std::swap(output, toMerge);
    } else {
        for (auto& emission : toMerge) {
            output.add_emission(std::move(emission));
        }
    }
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
