#pragma once

#include "emap/country.h"
#include "infra/string.h"

#include <string_view>
#include <unordered_set>

namespace emap {

class IgnoredName
{
public:
    IgnoredName() noexcept = default;
    IgnoredName(std::string_view name, std::unordered_set<CountryId> countryExceptions)
    : _name(name)
    , _countryExceptions(std::move(countryExceptions))
    {
    }

    bool is_ignored_for_country(std::string_view name, CountryId country) const noexcept
    {
        return is_ignored(name) && _countryExceptions.count(country) == 0;
    }

private:
    bool is_ignored(std::string_view name) const noexcept
    {
        return inf::str::iequals(_name, name);
    }

    std::string _name;
    std::unordered_set<CountryId> _countryExceptions;
};

}
