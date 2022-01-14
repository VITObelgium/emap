#pragma once

#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <string_view>
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

private:
    std::string _code;
    std::string _name;
};

class PollutantInventory
{
public:
    PollutantInventory(std::vector<Pollutant> pollutants, InputConversions conversions);

    Pollutant pollutant_from_string(std::string_view str) const;
    std::optional<Pollutant> try_pollutant_from_string(std::string_view str) const noexcept;
    size_t pollutant_count() const noexcept;

    std::span<const Pollutant> pollutants() const noexcept;

private:
    std::vector<Pollutant> _pollutants;
    InputConversions _conversions;
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
