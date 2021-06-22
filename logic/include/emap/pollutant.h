#pragma once

#include <string_view>

namespace emap {

enum class Pollutant
{
    CO,
    NH3,
    NMVOC,
    NOx,
    PM10,
    PM2_5,
    PMcoarse,
    SOx,
    Count,
};

Pollutant pollutant_from_string(std::string_view str);

}
