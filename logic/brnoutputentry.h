#pragma once

#include "infra/filesystem.h"
#include "infra/span.h"

#include "emap/pollutant.h"

namespace emap {

struct BrnOutputEntry
{
    int64_t x_m  = 0;
    int64_t y_m  = 0;
    double q_gs  = 0.0;
    double hc_MW = 0.0;
    double h_m   = 0.0;
    int32_t d_m  = 0;
    double s_m   = 0.0;
    int32_t dv   = 0;
    int32_t cat  = 0;
    int32_t area = 0;
    int32_t sd   = 0;
    std::string comp;
    double temp = 9999.0;
    double flow = 9999.0;
};

}