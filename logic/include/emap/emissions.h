#pragma once

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

class EmissionSector
{
public:
    enum class Type
    {
        Nfr,
        Gnfr,
    };

    EmissionSector() = default;
    EmissionSector(Type type, std::string_view name)
    : _type(type)
    , _name(name)
    {
    }

    Type type() const noexcept
    {
        return _type;
    }

    std::string_view name() const noexcept
    {
        return _name;
    }

private:
    Type _type = Type::Nfr;
    std::string _name;
};

class EmissionValue
{
public:
    EmissionValue() = default;
    EmissionValue(double amount, std::string_view unit)
    : _amount(amount)
    , _unit(unit)
    {
    }

    double amount() const noexcept
    {
        return _amount;
    }

    std::string_view unit() const noexcept
    {
        return _unit;
    }

private:
    double _amount = 0.0;
    std::string _unit;
};

struct EmissionInfo
{
    std::string country;
    EmissionSector sector;
    std::string pollutant;
    EmissionValue value;
    std::optional<Coordinate> coordinate;
};

class Emissions
{
public:
    void add_emission(EmissionInfo&& info);
    size_t size() const noexcept;

    auto begin() const noexcept
    {
        return _emissions.begin();
    }

    auto end() const noexcept
    {
        return _emissions.end();
    }

private:
    std::vector<EmissionInfo> _emissions;
};

std::string_view emission_type_name(EmissionType type);
std::string_view emission_sector_type_name(EmissionSector::Type type);

}

namespace fmt {
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
