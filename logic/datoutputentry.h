#pragma once

#include "infra/cell.h"
#include "infra/coordinate.h"

#include <cstdint>
#include <vector>

namespace emap {

struct DatOutputEntry
{
    int32_t countryCode = 0;
    inf::Cell cell;
    std::vector<double> emissions;
};

struct DatPointSourceOutputEntry
{
    int32_t pig = 0;
    inf::Coordinate coordinate;
    int32_t countryCode = 0;
    int32_t sectorId    = 0;
    double temperature  = 0.0;
    double velocity     = 0.0;
    double height       = 0.0;
    double diameter     = 0.0;
    std::vector<double> emissions;
};

}