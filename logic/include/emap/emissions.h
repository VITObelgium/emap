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
}