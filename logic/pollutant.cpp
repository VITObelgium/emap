#include "emap/pollutant.h"

#include "enuminfo.h"
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

static constexpr std::array<EnumInfo<Pollutant::Id>, enum_count<Pollutant::Id>()> s_pollutants = {{
    {Pollutant::Id::CO, "CO", "Carbon monoxide"},
    {Pollutant::Id::NH3, "NH3", "Ammonia"},
    {Pollutant::Id::NMVOC, "NMVOC", "Non-methane volatile organic compounds"},
    {Pollutant::Id::NOx, "NOx", "Nitrogen oxides (NO and NO2)"},
    {Pollutant::Id::PM10, "PM10", "Particulate matter (diameter < 10µm)"},
    {Pollutant::Id::PM2_5, "PM2.5", "Particulate matter (diameter < 2.5µm)"},
    {Pollutant::Id::PMcoarse, "PMcoarse", "Particulate matter (2.5µm < diameter < 10µm)"},
    {Pollutant::Id::SOx, "SOx", "Sulphur oxides (SOx as SO2)"},
    {Pollutant::Id::TSP, "TSP", "Total Suspended Particles"},
    {Pollutant::Id::BC, "BC", "Black Carbon"},
    {Pollutant::Id::Pb, "Pb", "Lead"},
    {Pollutant::Id::Cd, "Cd", "Cadmium"},
    {Pollutant::Id::Hg, "Hg", "Mercury"},
    {Pollutant::Id::As, "As", "Arsenic"},
    {Pollutant::Id::Cr, "Cr", "Chromium"},
    {Pollutant::Id::Cu, "Cu", "Copper"},
    {Pollutant::Id::Ni, "Ni", "Nickel"},
    {Pollutant::Id::Se, "Se", "Selenium"},
    {Pollutant::Id::Zn, "Zn", "Zinc"},
    {Pollutant::Id::PCDD_PCDF, "PCDD-PCDF", "PCDD/ PCDF (dioxins/ furans)"},
    {Pollutant::Id::BaP, "BaP", "benzo(a)pyrene"},
    {Pollutant::Id::BbF, "BbF", "benzo(b)fluoranthene"},
    {Pollutant::Id::BkF, "BkF", "benzo(k)fluoranthene"},
    {Pollutant::Id::Indeno, "Indeno", "Indeno (1,2,3-cd) pyrene"},
    {Pollutant::Id::PAHs, "PAHs", "4 EMEP PAHs"},
    {Pollutant::Id::HCB, "HCB", "Hexachlorobenzene"},
    {Pollutant::Id::PCBs, "PCBs", "Polychlorinated buphenyls"},
}};

Pollutant::Pollutant(Id id) noexcept
: _id(id)
{
}

Pollutant Pollutant::from_string(std::string_view str)
{
    Pollutant::Id id = Pollutant::Id::Invalid;

    const auto strLowerCase = str::lowercase(str);

    if (strLowerCase == "co") {
        id = Pollutant::Id::CO;
    } else if (strLowerCase == "nh3") {
        id = Pollutant::Id::NH3;
    } else if (strLowerCase == "nmvoc" || strLowerCase == "nmvos" || strLowerCase == "totaal nmvos") {
        id = Pollutant::Id::NMVOC;
    } else if (str::starts_with(strLowerCase, "nox")) {
        id = Pollutant::Id::NOx;
    } else if (strLowerCase == "pm10") {
        id = Pollutant::Id::PM10;
    } else if (strLowerCase == "pm2.5" || strLowerCase == "pm2,5" || strLowerCase == "pm2_5") {
        id = Pollutant::Id::PM2_5;
    } else if (strLowerCase == "pmcoarse") {
        id = Pollutant::Id::PMcoarse;
    } else if (str::starts_with(strLowerCase, "sox") || strLowerCase == "so2") {
        id = Pollutant::Id::SOx;
    } else if (strLowerCase == "tsp") {
        id = Pollutant::Id::TSP;
    } else if (strLowerCase == "bc") {
        id = Pollutant::Id::BC;
    } else if (strLowerCase == "pb") {
        id = Pollutant::Id::Pb;
    } else if (strLowerCase == "cd") {
        id = Pollutant::Id::Cd;
    } else if (strLowerCase == "hg") {
        id = Pollutant::Id::Hg;
    } else if (strLowerCase == "as") {
        id = Pollutant::Id::As;
    } else if (strLowerCase == "cr") {
        id = Pollutant::Id::Cr;
    } else if (strLowerCase == "cu") {
        id = Pollutant::Id::Cu;
    } else if (strLowerCase == "ni") {
        id = Pollutant::Id::Ni;
    } else if (strLowerCase == "se") {
        id = Pollutant::Id::Se;
    } else if (strLowerCase == "zn") {
        id = Pollutant::Id::Zn;
    } else if (strLowerCase == "pcdd-pcdf" || str::starts_with(strLowerCase, "pcdd/ pcdf")) {
        id = Pollutant::Id::PCDD_PCDF;
    } else if (strLowerCase == "bap" || strLowerCase == "benzo(a) pyrene" || strLowerCase == "benzo(a)") {
        id = Pollutant::Id::BaP;
    } else if (strLowerCase == "bbf" || strLowerCase == "benzo(b) fluoranthene" || strLowerCase == "benzo(b)") {
        id = Pollutant::Id::BbF;
    } else if (strLowerCase == "bkf" || strLowerCase == "benzo(k) fluoranthene" || strLowerCase == "benzo(k)") {
        id = Pollutant::Id::BkF;
    } else if (strLowerCase == "indeno" || strLowerCase == "Indeno (1,2,3-cd) pyrene") {
        id = Pollutant::Id::Indeno;
    } else if (strLowerCase == "pahs" || strLowerCase == "Total 1-4" || strLowerCase == "PAK 4") {
        id = Pollutant::Id::PAHs;
    } else if (strLowerCase == "hcb") {
        id = Pollutant::Id::HCB;
    } else if (strLowerCase == "pcbs" || strLowerCase == "pcb") {
        id = Pollutant::Id::PCBs;
    }

    if (id == Pollutant::Id::Invalid) {
        throw RuntimeError("Invalid pollutant name: '{}'", str);
    }

    return Pollutant(id);
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
