#include "emap/pollutant.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <array>

namespace emap {

using namespace inf;

static constexpr std::array<EnumInfo<Pollutant>, enum_value(Pollutant::Count)> s_pollutants = {{
    {Pollutant::CO, "CO", "Carbon monoxide"},
    {Pollutant::NH3, "NH3", "Ammonia"},
    {Pollutant::NMVOC, "NMVOC", "Non-methane volatile organic compounds"},
    {Pollutant::NOx, "NOx", "Nitrogen oxides (NO and NO2)"},
    {Pollutant::PM10, "PM10", "Particulate matter (diameter < 10µm)"},
    {Pollutant::PM2_5, "PM2.5", "Particulate matter (diameter < 2.5µm)"},
    {Pollutant::PMcoarse, "PMcoarse", "Particulate matter (2.5µm < diameter < 10µm)"},
    {Pollutant::SOx, "SOx", "Sulphur oxides"},
}};

Pollutant pollutant_from_string(std::string_view str)
{
    auto iter = std::find_if(s_pollutants.begin(), s_pollutants.end(), [str](const auto& pollutant) {
        return pollutant.serializedName == str;
    });

    if (iter != s_pollutants.end()) {
        return iter->id;
    }

    throw RuntimeError("Invalid pollutant name: '{}'", str);
}
}
