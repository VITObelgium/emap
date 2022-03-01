#pragma once

#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <string_view>
#include <type_safe/strong_typedef.hpp>

namespace emap {

struct CountryId : type_safe::strong_typedef<CountryId, int32_t>,
                   type_safe::strong_typedef_op::equality_comparison<CountryId>,
                   type_safe::strong_typedef_op::relational_comparison<CountryId>
{
    using strong_typedef::strong_typedef;
};

class Country
{
public:
    Country() noexcept
    {
    }

    Country(CountryId id, std::string_view isoCode, std::string_view label, bool isLand)
    : _id(id)
    , _isoCode(isoCode)
    , _label(label)
    , _isLand(isLand)
    {
    }

    const CountryId& id() const noexcept
    {
        return _id;
    }

    bool is_belgium() const noexcept
    {
        return iso_code() == "BEF" || iso_code() == "BEB" || iso_code() == "BEW";
    }

    bool is_sea() const noexcept
    {
        return !_isLand;
    }

    std::string_view iso_code() const noexcept
    {
        return static_cast<const std::string&>(_isoCode);
    }

    std::string_view full_name() const noexcept
    {
        return _label;
    }

    bool operator==(const Country& other) const noexcept
    {
        return _isoCode == other._isoCode;
    }

    bool operator!=(const Country& other) const noexcept
    {
        return !(*this == other);
    }

    std::string_view to_string() const noexcept;

private:
    CountryId _id = CountryId(0);
    std::string _isoCode;
    std::string _label;
    bool _isLand = true;
};

namespace country {
const Country BEF(CountryId(1), "BEF", "Flanders", true);
const Country BEB(CountryId(2), "BEB", "Brussels", true);
const Country BEW(CountryId(3), "BEW", "Wallonia", true);
}

class CountryInventory
{
public:
    CountryInventory(std::vector<Country> countries);

    Country country_from_string(std::string_view str) const;
    std::optional<Country> try_country_from_string(std::string_view str) const noexcept;
    size_t country_count() const noexcept;

    std::span<const Country> list() const noexcept;
    Country non_belgian_country() const;

private:
    std::vector<Country> _countries;
};

}

namespace std {
template <>
struct hash<emap::Country>
{
    size_t operator()(const emap::Country& country) const
    {
        return hash<std::string_view>()(country.iso_code());
    }
};
}

namespace fmt {
template <>
struct formatter<emap::Country>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::Country& val, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}", val.to_string());
    }
};
}

namespace std {
// we want to use it with the std::unordered_* containers
template <>
struct hash<emap::CountryId> : type_safe::hashable<emap::CountryId>
{
};

}