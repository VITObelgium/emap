#pragma once

#include <fmt/core.h>
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
    Invalid,
};

Pollutant pollutant_from_string(std::string_view str);
std::string_view to_string(Pollutant value) noexcept;
std::string_view to_description_string(Pollutant value) noexcept;

}

namespace fmt {
template <>
struct formatter<emap::Pollutant>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::Pollutant& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), emap::to_string(val));
    }
};
}
