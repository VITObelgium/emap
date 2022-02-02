#include "emap/country.h"

#include "enuminfo.h"
#include "infra/algo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>

namespace emap {

using namespace inf;

std::string_view Country::to_string() const noexcept
{
    return iso_code();
}

CountryInventory::CountryInventory(std::vector<Country> pollutants)
: _countries(std::move(pollutants))
{
}

Country CountryInventory::country_from_string(std::string_view str) const
{
    if (const auto country = try_country_from_string(str); country.has_value()) {
        return *country;
    }

    throw RuntimeError("Invalid country name: {}", str);
}

std::optional<Country> CountryInventory::try_country_from_string(std::string_view str) const noexcept
{
    const auto* country = find_in_container(_countries, [str](const Country& pol) {
        return pol.iso_code() == str;
    });

    return country ? *country : std::optional<Country>();
}

size_t CountryInventory::country_count() const noexcept
{
    return _countries.size();
}

std::span<const Country> CountryInventory::list() const noexcept
{
    return _countries;
}

Country CountryInventory::non_belgian_country() const
{
    for (auto& country : _countries) {
        if (!country.is_belgium() && !country.is_sea()) {
            return country;
        }
    }

    throw RuntimeError("No non belgian country configured");
}

}
