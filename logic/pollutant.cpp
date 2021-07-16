#include "emap/pollutant.h"

#include "enuminfo.h"
#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <array>
#include <cassert>
#include <type_traits>

namespace emap {

using namespace inf;

static constexpr std::array<EnumInfo<Pollutant::Id>, enum_count<Pollutant::Id>()> s_pollutants = {{
    {Pollutant::Id::CO, "CO", "Carbon monoxide"},
    {Pollutant::Id::NH3, "NH3", "Ammonia"},
    {Pollutant::Id::NMVOC, "NMVOC", "Non-methane volatile organic compounds"},
    {Pollutant::Id::NOx, "NOx", "Nitrogen oxides (NO and NO2)"},
    {Pollutant::Id::PM10, "PM10", "Particulate matter (diameter < 10µm)"},
    {Pollutant::Id::PM2_5, "PM2.5", "Particulate matter (diameter < 2.5µm)"},
    {Pollutant::Id::PMcoarse, "PMcoarse", "Particulate matter (2.5µm < diameter < 10µm)"},
    {Pollutant::Id::SOx, "SOx", "Sulphur oxides"},
}};

Pollutant::Pollutant(Id id) noexcept
: _id(id)
{
}

Pollutant Pollutant::from_string(std::string_view str)
{
    auto iter = std::find_if(s_pollutants.begin(), s_pollutants.end(), [str](const auto& pollutant) {
        return pollutant.serializedName == str;
    });

    if (iter != s_pollutants.end()) {
        return Pollutant(iter->id);
    }

    throw RuntimeError("Invalid pollutant name: '{}'", str);
}

Pollutant::Id Pollutant::id() const noexcept
{
    return _id;
}

std::string_view Pollutant::code() const noexcept
{
    assert(enum_value(_id) < truncate<std::underlying_type_t<Pollutant::Id>>(s_pollutants.size()));
    return s_pollutants[enum_value(_id)].serializedName;
}

std::string_view Pollutant::full_name() const noexcept
{
    assert(enum_value(_id) < truncate<std::underlying_type_t<Pollutant::Id>>(s_pollutants.size()));
    return s_pollutants[enum_value(_id)].description;
}

}
