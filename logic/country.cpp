#include "emap/country.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <array>

namespace emap {

using namespace inf;

static constexpr std::array<EnumInfo<Country>, enum_value(Country::Count)> s_countries = {{
    {Country::AL, "AL", "Albania"},
    {Country::AM, "AM", "Armenia"},
    {Country::ARE, "ARE", "Rest of Aral Lake in the extended EMEP domain"},
    {Country::ARO, "ARO", "Aral Lake in the former official EMEP domain"},
    {Country::ASE, "ASE", "Remaining asian areas in the extended EMEP domain"},
    {Country::ASM, "ASM", "Modified remaining asian areas in the former official EMEP domain"},
    {Country::AT, "AT", "Austria"},
    {Country::ATL, "ATL", "Remaining North-East Atlantic Ocean"},
    {Country::AZ, "AZ", "Azerbaijan"},
    {Country::BA, "BA", "Bosnia & Herzegovina"},
    {Country::BAS, "BAS", "Baltic Sea"},
    {Country::BEB, "BEB", "Brussels"},
    {Country::BEF, "BEF", "Flanders"},
    {Country::BEW, "BEW", "Wallonia"},
    {Country::BG, "BG", "Bulgaria"},
    {Country::BLS, "BLS", "Black Sea"},
    {Country::BY, "BY", "Belarus"},
    {Country::CAS, "CAS", "Caspian Sea"},
    {Country::CH, "CH", "Switzerland"},
    {Country::CY, "CY", "Cyprus"},
    {Country::CZ, "CZ", "Czechia"},
    {Country::DE, "DE", "Germany"},
    {Country::DK, "DK", "Denmark"},
    {Country::EE, "EE", "Estonia"},
    {Country::ES, "ES", "Spain"},
    {Country::FI, "FI", "Finland"},
    {Country::FR, "FR", "France"},
    {Country::GB, "GB", "United Kingdom"},
    {Country::GE, "GE", "Georgia"},
    {Country::GL, "GL", "Greenland"},
    {Country::GR, "GR", "Greece"},
    {Country::HR, "HR", "Croatia"},
    {Country::HU, "HU", "Hungary"},
    {Country::IE, "IE", "Ireland"},
    {Country::IS, "IS", "Iceland"},
    {Country::IT, "IT", "Italy"},
    {Country::KG, "KG", "Kyrgyzstan"},
    {Country::KZ, "KZ", "Kazakhstan"},
    {Country::KZE, "KZE", "Rest of Kazakhstan in the extended EMEP domain "},
    {Country::LI, "LI", "Liechtenstein"},
    {Country::LT, "LT", "Lithuania"},
    {Country::LU, "LU", "Luxembourg"},
    {Country::LV, "LV", "Latvia"},
    {Country::MC, "MC", "Monaco"},
    {Country::MD, "MD", "Moldova, Republic of"},
    {Country::ME, "ME", "Montenegro"},
    {Country::MED, "MED", "Mediterranean Sea"},
    {Country::MK, "MK", "North Macedonia"},
    {Country::MT, "MT", "Malta"},
    {Country::NL, "NL", "Netherlands"},
    {Country::NO, "NO", "Norway"},
    {Country::NOA, "NOA", "North Africa"},
    {Country::NOS, "NOS", "North Sea"},
    {Country::PL, "PL", "Poland"},
    {Country::PT, "PT", "Portugal"},
    {Country::RFE, "RFE", "Rest of Russion federation in the extended EMEP domain"},
    {Country::RO, "RO", "Romania"},
    {Country::RS, "RS", "Serbia"},
    {Country::RU, "RU", "Russian Federation"},
    {Country::RUX, "RUX", "EMEP-External part of Russion federation"},
    {Country::SE, "SE", "Sweden"},
    {Country::SI, "SI", "Slovenia"},
    {Country::SK, "SK", "Slovakia"},
    {Country::TJ, "TJ", "Tajikistan"},
    {Country::TME, "TME", "Rest of Turkmenistan in the extended EMEP domain "},
    {Country::TMO, "TMO", "Turkmenistan in the former official EMEP domain"},
    {Country::TR, "TR", "Turkey"},
    {Country::UA, "UA", "Ukraine"},
    {Country::UZE, "UZE", "Rest of Uzbekistan in the extended EMEP domain"},
    {Country::UZO, "UZO", "Uzbekistan in the former official EMEP domain"},
}};

Country country_from_string(std::string_view str)
{
    auto iter = std::find_if(s_countries.begin(), s_countries.end(), [str](const auto& pollutant) {
        return pollutant.serializedName == str;
    });

    if (iter != s_countries.end()) {
        return iter->id;
    }

    throw RuntimeError("Invalid pollutant name: '{}'", str);
}
}
