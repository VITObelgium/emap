#pragma once

#include <fmt/core.h>
#include <optional>
#include <string_view>

namespace emap {

class Country
{
public:
    enum class Id
    {
        AL,
        AM,
        ARE,
        ARO,
        ASE,
        ASM,
        AT,
        ATL,
        AZ,
        BA,
        BAS,
        BEB,
        BEF,
        BEW,
        BG,
        BLS,
        BY,
        CAS,
        CH,
        CY,
        CZ,
        DE,
        DK,
        EE,
        ES,
        FI,
        FR,
        GB,
        GE,
        GL,
        GR,
        HR,
        HU,
        IE,
        IS,
        IT,
        KG,
        KZ,
        KZE,
        LI,
        LT,
        LU,
        LV,
        MC,
        MD,
        ME,
        MED,
        MK,
        MT,
        NL,
        NO,
        NOA,
        NOS,
        PL,
        PT,
        RFE,
        RO,
        RS,
        RU,
        RUX,
        SE,
        SI,
        SK,
        TJ,
        TME,
        TMO,
        TR,
        UA,
        UZE,
        UZO,
        EnumCount,
        Invalid,
    };

    static Country from_string(std::string_view str);
    static std::optional<Country> try_from_string(std::string_view str) noexcept;

    constexpr Country() noexcept = default;
    constexpr Country(Id id)
    : _id(id)
    {
    }

    constexpr bool is_belgium() const noexcept
    {
        return _id == Id::BEF || _id == Id::BEB || _id == Id::BEW;
    }

    constexpr bool is_sea() const noexcept
    {
        return _id == Id::ARE ||
               _id == Id::ARO ||
               _id == Id::ATL ||
               _id == Id::BAS ||
               _id == Id::BLS ||
               _id == Id::CAS ||
               _id == Id::MED ||
               _id == Id::NOS;
    }

    constexpr Id id() const noexcept
    {
        return _id;
    }

    constexpr bool operator==(const Country& other) const noexcept
    {
        return _id == other._id;
    }

    constexpr bool operator!=(const Country& other) const noexcept
    {
        return !(*this == other);
    }

    std::string_view to_string() const noexcept;
    std::string_view code() const noexcept;
    std::string_view full_name() const noexcept;
    bool included() const noexcept;

private:
    Id _id = Id::Invalid;
};

}

namespace fmt {
template <>
struct formatter<emap::Country>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::Country& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), val.to_string());
    }
};
}
