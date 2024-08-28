#pragma once

#include "emap/country.h"
#include "emap/ignoredname.h"
#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace emap {

class Pollutant
{
public:
    Pollutant() = default;
    Pollutant(std::string_view code, std::string_view name)
    : _code(code)
    , _name(name)
    {
    }

    std::string_view code() const noexcept
    {
        return _code;
    }

    std::string_view full_name() const noexcept
    {
        return _name;
    }

    bool operator==(const Pollutant& other) const noexcept
    {
        return _code == other._code;
    }

    bool operator!=(const Pollutant& other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::string _code;
    std::string _name;
};

}

namespace std {
template <>
struct hash<emap::Pollutant>
{
    size_t operator()(const emap::Pollutant& pollutant) const
    {
        return hash<std::string_view>()(pollutant.code());
    }
};
}

namespace emap {

class PollutantInventory
{
public:
    PollutantInventory(std::vector<Pollutant> pollutants, InputConversions conversions, std::vector<IgnoredName> ignoredPollutants);

    Pollutant pollutant_from_string(std::string_view str) const;
    std::optional<Pollutant> try_pollutant_from_string(std::string_view str) const noexcept;
    size_t pollutant_count() const noexcept;

    /* Get the optional fallback pollutant for the given pollutant
     * in some circumanstance the configured fallback pollutant is used when there is no pollutant data present */
    std::optional<Pollutant> pollutant_fallback(const Pollutant& pollutant) const noexcept;

    void add_fallback_for_pollutant(const Pollutant& pollutant, const Pollutant& fallback);
    bool is_ignored_pollutant(std::string_view str, const Country& country) const noexcept;

    std::span<const Pollutant> list() const noexcept;

private:
    std::vector<Pollutant> _pollutants;
    std::unordered_map<Pollutant, Pollutant> _pollutantFallbacks;
    std::vector<IgnoredName> _ignoredPollutants;
    InputConversions _conversions;
};

}

template <>
struct fmt::formatter<emap::Pollutant>
{
    FMT_CONSTEXPR20 auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        return ctx.begin();
    }

    auto format(const emap::Pollutant& val, format_context& ctx) const -> format_context::iterator
    {
        return fmt::format_to(ctx.out(), "{}", val.code());
    }
};
