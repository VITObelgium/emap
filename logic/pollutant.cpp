#include "emap/pollutant.h"

#include "enuminfo.h"
#include "infra/algo.h"
#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/exception.h"
#include "infra/string.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <type_traits>

namespace emap {

using namespace inf;

PollutantInventory::PollutantInventory(std::vector<Pollutant> pollutants, InputConversions conversions, std::vector<std::string> ignoredPollutants)
: _pollutants(std::move(pollutants))
, _ignoredPollutants(std::move(ignoredPollutants))
, _conversions(std::move(conversions))
{
}

Pollutant PollutantInventory::pollutant_from_string(std::string_view str) const
{
    if (const auto pollutant = try_pollutant_from_string(str); pollutant.has_value()) {
        return *pollutant;
    }

    throw RuntimeError("Invalid pollutant name: '{}'", str);
}

std::optional<Pollutant> PollutantInventory::try_pollutant_from_string(std::string_view str) const noexcept
{
    auto pollutantCode = _conversions.lookup(str);
    if (pollutantCode.empty()) {
        pollutantCode = str; // not all valid names have to be present in the conversion table
    }

    const auto* pollutant = find_in_container(_pollutants, [pollutantCode](const Pollutant& pol) {
        return str::iequals(pol.code(), pollutantCode);
    });

    return pollutant ? *pollutant : std::optional<Pollutant>();
}

size_t PollutantInventory::pollutant_count() const noexcept
{
    return _pollutants.size();
}

std::optional<Pollutant> PollutantInventory::pollutant_fallback(const Pollutant& pollutant) const noexcept
{
    return inf::find_in_map_optional(_pollutantFallbacks, pollutant);
}

std::span<const Pollutant> PollutantInventory::list() const noexcept
{
    return _pollutants;
}

void PollutantInventory::add_fallback_for_pollutant(const Pollutant& pollutant, const Pollutant& fallback)
{
    _pollutantFallbacks[pollutant] = fallback;
}

bool PollutantInventory::is_ignored_pollutant(std::string_view str) const noexcept
{
    return std::any_of(_ignoredPollutants.begin(), _ignoredPollutants.end(), [=](const std::string& ign) {
        return str::iequals(ign, str);
    });
}

}
