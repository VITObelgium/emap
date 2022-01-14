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

PollutantInventory::PollutantInventory(std::vector<Pollutant> pollutants, InputConversions conversions)
: _pollutants(std::move(pollutants))
, _conversions(std::move(conversions))
{
}

Pollutant PollutantInventory::pollutant_from_string(std::string_view str) const
{
    if (const auto pollutant = try_pollutant_from_string(_conversions.lookup(str)); pollutant.has_value()) {
        return *pollutant;
    }

    throw RuntimeError("Invalid pollutant name: {}", str);
}

std::optional<Pollutant> PollutantInventory::try_pollutant_from_string(std::string_view str) const noexcept
{
    const auto* pollutant = find_in_container(_pollutants, [str](const Pollutant& pol) {
        return pol.code() == str;
    });

    return pollutant ? *pollutant : std::optional<Pollutant>();
}

size_t PollutantInventory::pollutant_count() const noexcept
{
    return _pollutants.size();
}

std::span<const Pollutant> PollutantInventory::pollutants() const noexcept
{
    return _pollutants;
}
}
