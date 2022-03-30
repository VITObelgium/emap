#pragma once

#include "infra/exception.h"
#include "infra/string.h"

#include <string_view>

namespace emap {

inline std::optional<double> to_giga_gram_factor(std::string_view unit) noexcept
{
    std::optional<double> result;

    auto trimmedUnit = inf::str::trimmed_view(unit);

    if (trimmedUnit == "Gg" || trimmedUnit == "kt") {
        return result = 1.0;
    }

    if (trimmedUnit == "ton" || trimmedUnit == "t" || trimmedUnit == "t/jr" || trimmedUnit == "t/yr" || trimmedUnit == "Mg") {
        return 1.0 / 1000.0;
    }

    if (trimmedUnit == "g I-TEQ" || trimmedUnit == "g") {
        return 1.0 / 1e15;
    }

    return result;
}

inline double to_giga_gram(double value, std::string_view unit)
{
    auto factor = to_giga_gram_factor(unit);
    if (!factor.has_value()) {
        throw inf::RuntimeError("Unexpected unit: '{}', no conversion rule defined", unit);
    }

    return value * (*factor);
}

}
