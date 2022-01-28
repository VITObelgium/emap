#pragma once

#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <string_view>
#include <type_safe/strong_typedef.hpp>

namespace emap {

struct CountryId : type_safe::strong_typedef<CountryId, std::string>,
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

    Country(std::string_view isoCode, std::string_view label, bool isLand)
    : _isoCode(std::string(isoCode))
    , _label(label)
    , _isLand(isLand)
    {
    }

    const CountryId& id() const noexcept
    {
        return _isoCode;
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
    CountryId _isoCode = CountryId("");
    std::string _label;
    bool _isLand = true;
};

namespace country {
const Country BEB("BEB", "Brussels", true);
const Country BEF("BEF", "Flanders", true);
const Country BEW("BEW", "Wallonia", true);
}

class CountryInventory
{
public:
    CountryInventory(std::vector<Country> countries);

    Country country_from_string(std::string_view str) const;
    std::optional<Country> try_country_from_string(std::string_view str) const noexcept;
    size_t country_count() const noexcept;

    std::span<const Country> countries() const noexcept;
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

namespace std {
// we want to use it with the std::unordered_* containers
template <>
struct hash<emap::CountryId> : type_safe::hashable<emap::CountryId>
{
};

}