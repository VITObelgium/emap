#include "emap/country.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <array>

namespace emap {

using namespace inf;

static constexpr std::array<EnumInfo<Country>, enum_value(Country::Id::Count)> s_countries = {{
    {Country::Id::AL, "AL", "Albania"},
    {Country::Id::AM, "AM", "Armenia"},
    {Country::Id::ARE, "ARE", "Rest of Aral Lake in the extended EMEP domain"},
    {Country::Id::ARO, "ARO", "Aral Lake in the former official EMEP domain"},
    {Country::Id::ASE, "ASE", "Remaining asian areas in the extended EMEP domain"},
    {Country::Id::ASM, "ASM", "Modified remaining asian areas in the former official EMEP domain"},
    {Country::Id::AT, "AT", "Austria"},
    {Country::Id::ATL, "ATL", "Remaining North-East Atlantic Ocean"},
    {Country::Id::AZ, "AZ", "Azerbaijan"},
    {Country::Id::BA, "BA", "Bosnia & Herzegovina"},
    {Country::Id::BAS, "BAS", "Baltic Sea"},
    {Country::Id::BEB, "BEB", "Brussels"},
    {Country::Id::BEF, "BEF", "Flanders"},
    {Country::Id::BEW, "BEW", "Wallonia"},
    {Country::Id::BG, "BG", "Bulgaria"},
    {Country::Id::BLS, "BLS", "Black Sea"},
    {Country::Id::BY, "BY", "Belarus"},
    {Country::Id::CAS, "CAS", "Caspian Sea"},
    {Country::Id::CH, "CH", "Switzerland"},
    {Country::Id::CY, "CY", "Cyprus"},
    {Country::Id::CZ, "CZ", "Czechia"},
    {Country::Id::DE, "DE", "Germany"},
    {Country::Id::DK, "DK", "Denmark"},
    {Country::Id::EE, "EE", "Estonia"},
    {Country::Id::ES, "ES", "Spain"},
    {Country::Id::FI, "FI", "Finland"},
    {Country::Id::FR, "FR", "France"},
    {Country::Id::GB, "GB", "United Kingdom"},
    {Country::Id::GE, "GE", "Georgia"},
    {Country::Id::GL, "GL", "Greenland"},
    {Country::Id::GR, "GR", "Greece"},
    {Country::Id::HR, "HR", "Croatia"},
    {Country::Id::HU, "HU", "Hungary"},
    {Country::Id::IE, "IE", "Ireland"},
    {Country::Id::IS, "IS", "Iceland"},
    {Country::Id::IT, "IT", "Italy"},
    {Country::Id::KG, "KG", "Kyrgyzstan"},
    {Country::Id::KZ, "KZ", "Kazakhstan"},
    {Country::Id::KZE, "KZE", "Rest of Kazakhstan in the extended EMEP domain "},
    {Country::Id::LI, "LI", "Liechtenstein"},
    {Country::Id::LT, "LT", "Lithuania"},
    {Country::Id::LU, "LU", "Luxembourg"},
    {Country::Id::LV, "LV", "Latvia"},
    {Country::Id::MC, "MC", "Monaco"},
    {Country::Id::MD, "MD", "Moldova, Republic of"},
    {Country::Id::ME, "ME", "Montenegro"},
    {Country::Id::MED, "MED", "Mediterranean Sea"},
    {Country::Id::MK, "MK", "North Macedonia"},
    {Country::Id::MT, "MT", "Malta"},
    {Country::Id::NL, "NL", "Netherlands"},
    {Country::Id::NO, "NO", "Norway"},
    {Country::Id::NOA, "NOA", "North Africa"},
    {Country::Id::NOS, "NOS", "North Sea"},
    {Country::Id::PL, "PL", "Poland"},
    {Country::Id::PT, "PT", "Portugal"},
    {Country::Id::RFE, "RFE", "Rest of Russion federation in the extended EMEP domain"},
    {Country::Id::RO, "RO", "Romania"},
    {Country::Id::RS, "RS", "Serbia"},
    {Country::Id::RU, "RU", "Russian Federation"},
    {Country::Id::RUX, "RUX", "EMEP-External part of Russion federation"},
    {Country::Id::SE, "SE", "Sweden"},
    {Country::Id::SI, "SI", "Slovenia"},
    {Country::Id::SK, "SK", "Slovakia"},
    {Country::Id::TJ, "TJ", "Tajikistan"},
    {Country::Id::TME, "TME", "Rest of Turkmenistan in the extended EMEP domain "},
    {Country::Id::TMO, "TMO", "Turkmenistan in the former official EMEP domain"},
    {Country::Id::TR, "TR", "Turkey"},
    {Country::Id::UA, "UA", "Ukraine"},
    {Country::Id::UZE, "UZE", "Rest of Uzbekistan in the extended EMEP domain"},
    {Country::Id::UZO, "UZO", "Uzbekistan in the former official EMEP domain"},
}};

Country Country::from_string(std::string_view str)
{
    auto iter = std::find_if(s_countries.begin(), s_countries.end(), [str](const auto& pollutant) {
        return pollutant.serializedName == str;
    });

    if (iter != s_countries.end()) {
        return Country(iter->id);
    }

    throw RuntimeError("Invalid pollutant name: '{}'", str);
}
std::string_view Country::to_string() const noexcept
{
    return s_countries[enum_value(_id)].serializedName;
}

std::string_view Country::full_name() const noexcept
{
    return s_countries[enum_value(_id)].description;
}
}
