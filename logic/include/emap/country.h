#pragma once

#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <string_view>

namespace emap {

class Country
{
public:
    Country() noexcept = default;
    Country(std::string_view isoCode, std::string_view label, bool isLand)
    : _isoCode(isoCode)
    , _label(label)
    , _isLand(isLand)
    {
    }

    bool is_belgium() const noexcept
    {
        return _isoCode == "BEF" || _isoCode == "BEB" || _isoCode == "BEW";
    }

    bool is_sea() const noexcept
    {
        return !_isLand;
    }

    std::string_view iso_code() const noexcept
    {
        return _isoCode;
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
    std::string _isoCode;
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
