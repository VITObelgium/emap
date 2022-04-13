#pragma once

#include <cstdint>

namespace emap {

struct SectorParameterConfig
{
    double hc_MW = 0.0;
    double h_m   = 0.0;
    double s_m   = 0.0;
    double tb    = 0.0;
    int32_t id   = 0;
};

}