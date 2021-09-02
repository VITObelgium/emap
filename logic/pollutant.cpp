#include "emap/pollutant.h"

#include "enuminfo.h"
#include "infra/cast.h"
#include "infra/string.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

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

    if (str == "CO") {
        id = Pollutant::Id::CO;
    } else if (str == "NH3") {
        id = Pollutant::Id::NH3;
    } else if (str == "NMVOC" || str == "NMVOS" || str == "Totaal NMVOS") {
        id = Pollutant::Id::NMVOC;
    } else if (str::starts_with(str, "NOx")) {
        id = Pollutant::Id::NOx;
    } else if (str == "PM10") {
        id = Pollutant::Id::PM10;
    } else if (str == "PM2.5" || str == "PM2,5") {
        id = Pollutant::Id::PM2_5;
    } else if (str == "PMcoarse") {
        id = Pollutant::Id::PMcoarse;
    } else if (str::starts_with(str, "SOx")) {
        id = Pollutant::Id::SOx;
    } else if (str == "TSP") {
        id = Pollutant::Id::TSP;
    } else if (str == "BC") {
        id = Pollutant::Id::BC;
    } else if (str == "Pb") {
        id = Pollutant::Id::Pb;
    } else if (str == "Cd") {
        id = Pollutant::Id::Cd;
    } else if (str == "Hg") {
        id = Pollutant::Id::Hg;
    } else if (str == "As") {
        id = Pollutant::Id::As;
    } else if (str == "Cr") {
        id = Pollutant::Id::Cr;
    } else if (str == "Cu") {
        id = Pollutant::Id::Cu;
    } else if (str == "Ni") {
        id = Pollutant::Id::Ni;
    } else if (str == "Se") {
        id = Pollutant::Id::Se;
    } else if (str == "Zn") {
        id = Pollutant::Id::Zn;
    } else if (str == "PCDD-PCDF" || str::starts_with(str, "PCDD/ PCDF")) {
        id = Pollutant::Id::PCDD_PCDF;
    } else if (str == "BaP" || str == "benzo(a) pyrene") {
        id = Pollutant::Id::BaP;
    } else if (str == "BbF" || str == "benzo(b) fluoranthene") {
        id = Pollutant::Id::BbF;
    } else if (str == "BkF" || str == "benzo(k) fluoranthene") {
        id = Pollutant::Id::BkF;
    } else if (str == "Indeno" || str == "Indeno (1,2,3-cd) pyrene") {
        id = Pollutant::Id::Indeno;
    } else if (str == "PAHs" || str == "Total 1-4" || str == "PAK 4") {
        id = Pollutant::Id::PAHs;
    } else if (str == "HCB") {
        id = Pollutant::Id::HCB;
    } else if (str == "PCBs") {
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
