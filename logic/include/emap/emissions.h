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

    std::vector<EmissionEntry> scaled_point_emissions() const noexcept
    {
        std::vector<EmissionEntry> result;
        result.reserve(_pointEmissionEntries.size());
        std::transform(_pointEmissionEntries.begin(), _pointEmissionEntries.end(), std::back_inserter(result), [=](const EmissionEntry& entry) {
            auto scaledEntry = entry;
            scaledEntry.set_value(entry.value() * _pointScaling);
            return entry;
        });
        return result;
    }

    double scaled_total_emissions_sum() const noexcept
    {
        return scaled_point_emissions_sum() + scaled_diffuse_emissions_sum();
    }

    double scaled_diffuse_emissions_sum() const noexcept
    {
        return _diffuseEmission * _diffuseScaling;
    }

    double scaled_point_emissions_sum() const noexcept
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
    using value_type     = TEmission;
    using size_type      = std::size_t;
    using pointer        = TEmission*;
    using const_pointer  = const TEmission*;
    using iterator       = typename std::vector<TEmission>::iterator;
    using const_iterator = typename std::vector<TEmission>::const_iterator;

    EmissionCollection(date::year year)
    : _year(year)
    {
    }

    EmissionCollection(date::year year, std::vector<TEmission> emissions)
    : _year(year)
    {
        set_emissions(std::move(emissions));
    }

    date::year year() const noexcept
    {
        return _year;
    }

    void add_emission(TEmission&& info)
    {
        // Make sure the emissions remain sorted
        auto emissionIter = find_sorted(info.id());
        _emissions.insert(emissionIter, std::move(info));
    }

    void add_emissions(std::span<const TEmission> emissions)
    {
        // Make sure the emissions remain sorted
        inf::append_to_container(_emissions, emissions);
        sort_emissions();
    }

    void set_emissions(std::vector<TEmission> emissions)
    {
        // Sort the emissions after assignment
        _emissions = std::move(emissions);
        sort_emissions();
    }

    void update_emission(TEmission&& info)
    {
        auto emissionIter = find_sorted(info.id());
        if (emissionIter != _emissions.end() && emissionIter->id() == info.id()) {
            *emissionIter = info;
        } else {
            throw inf::RuntimeError("Update of non existing emission");
        }
    }

    void update_or_add_emission(TEmission&& info)
    {
        auto emissionIter = find_sorted(info.id());
        if (emissionIter != _emissions.end() && emissionIter->id() == info.id()) {
            // update the existing emission
            *emissionIter = std::forward<TEmission&&>(info);
        } else {
            _emissions.insert(emissionIter, std::forward<TEmission&&>(info));
        }
    }

    const TEmission& emission_with_id(const EmissionIdentifier& id) const
    {
        auto emissionIter = find_sorted(id);
        if (emissionIter != _emissions.end() && emissionIter->id() == id) {
            return *emissionIter;
        }

        throw inf::RuntimeError("No emission found with id: {}", id);
    }

    std::optional<TEmission> try_emission_with_id(const EmissionIdentifier& id) const noexcept
    {
        auto emissionIter = find_sorted(id);
        if (emissionIter != _emissions.end() && emissionIter->id() == id) {
            return *emissionIter;
        }

        return {};
    }

    std::vector<TEmission> emissions_with_id(const EmissionIdentifier& id) const
    {
        std::vector<TEmission> result;
        std::copy_if(_emissions.begin(), _emissions.end(), std::back_inserter(result), [&id](const TEmission& em) {
            return em.id() == id;
        });

        return result;
    }

    size_t empty() const noexcept
    {
        return _emissions.empty();
    }

    auto data() const
    {
        return _emissions.data();
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
    void sort_emissions()
    {
        std::sort(_emissions.begin(), _emissions.end(), [](const TEmission& lhs, const TEmission& rhs) {
            return lhs.id() < rhs.id();
        });
    }

    auto find_sorted(const EmissionIdentifier& id)
    {
        return std::lower_bound(_emissions.begin(), _emissions.end(), id, [](const TEmission& lhs, const EmissionIdentifier& id) {
            return lhs.id() < id;
        });
    }

    auto find_sorted(const EmissionIdentifier& id) const
    {
        return std::lower_bound(_emissions.begin(), _emissions.end(), id, [](const TEmission& lhs, const EmissionIdentifier& id) {
            return lhs.id() < id;
        });
    }

    date::year _year;
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
            output.update_or_add_emission(std::move(emission));
        }
    }
}

template <typename T>
void merge_unique_emissions(EmissionCollection<T>& output, EmissionCollection<T>&& toMerge)
{
    if (output.empty()) {
        std::swap(output, toMerge);
    } else {
        output.add_emissions(std::forward<EmissionCollection<T>>(toMerge));
    }
}

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
