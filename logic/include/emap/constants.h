#pragma once

#include <string_view>

namespace emap::constants {

namespace pollutant {

inline const std::string_view PM10     = "PM10";
inline const std::string_view PM2_5    = "PM2.5";
inline const std::string_view PMCoarse = "PMcoarse";

}

inline constexpr const int64_t secondsPerYear    = 31'536'000;
inline constexpr const double toGramPerYearRatio = 1'000'000'000.0 / secondsPerYear;

}
