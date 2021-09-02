#pragma once

#include <fmt/core.h>
#include <string_view>

namespace emap {

class Pollutant
{
public:
    enum class Id
    {
        CO,
        NH3,
        NMVOC,
        NOx,
        PM10,
        PM2_5,
        PMcoarse,
        SOx,
        TSP,
        BC,
        Pb,
        Cd,
        Hg,
        As,
        Cr,
        Cu,
        Ni,
        Se,
        Zn,
        PCDD_PCDF,
        BaP,
        BbF,
        BkF,
        Indeno,
        PAHs,
        HCB,
        PCBs,
        EnumCount,
        Invalid,
    };

    static Pollutant from_string(std::string_view str);

    Pollutant() noexcept = default;
    Pollutant(Id id) noexcept;

    Pollutant::Id id() const noexcept;
    std::string_view code() const noexcept;
    std::string_view full_name() const noexcept;

    constexpr bool operator==(const Pollutant& other) const noexcept
    {
        return _id == other._id;
    }

    constexpr bool operator!=(const Pollutant& other) const noexcept
    {
        return !(*this == other);
    }

private:
    Id _id = Id::Invalid;
};

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
        return format_to(ctx.out(), val.code());
    }
};
}
