#pragma once

#include "infra/exception.h"

#include <string_view>

namespace emap {

inline double to_giga_gram(double value, std::string_view unit)
{
    if (unit == "Gg") {
        return value;
    }
    
    if (unit == "ton" || unit == "t" || unit == "t/jr") {
        return value / 1000.0;
    }

    throw inf::RuntimeError("Unexpected unit: '{}', no conversion rule defined", unit);
}

}
