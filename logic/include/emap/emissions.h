#pragma once

#include <date/date.h>
#include <string>
#include <vector>

namespace emap {

enum class EmissionType
{
    Historic,
    Future,
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
    std::string nfrSector;
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