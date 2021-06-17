#pragma once

#include <date/date.h>
#include <string>
#include <string_view>
#include <vector>

namespace emap {

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

struct EmissionValue
{
    EmissionValue() = default;
    EmissionValue(double val, std::string_view un)
    : value(val)
    , unit(un)
    {
    }

    double value = 0.0;
    std::string unit;
};

struct EmissionInfo
{
    EmissionType type = EmissionType::Historic;
    std::string scenario;
    date::year year;
    date::year reportingYear;
    std::string country;
    EmissionSector sector;
    std::string pollutant;
    EmissionValue value;
};

class Emissions
{
public:
    void add_emission(EmissionInfo&& info);

private:
    std::vector<EmissionInfo> _emissions;
};
}