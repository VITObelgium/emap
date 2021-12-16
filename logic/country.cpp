#include "emap/country.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>

namespace emap {

using namespace inf;

struct CountryInfo : public EnumInfo<Country>
{
    bool included = false;
    //int32_t code  = 0;
};

static constexpr std::array<CountryInfo, enum_count<Country::Id>()> s_countries = {{
    {Country::Id::AL, "AL", "Albania", true},
    {Country::Id::AM, "AM", "Armenia", true},
    {Country::Id::ARE, "ARE", "Rest of Aral Lake in the extended EMEP domain", false},
    {Country::Id::ARO, "ARO", "Aral Lake in the former official EMEP domain", false},
    {Country::Id::ASE, "ASE", "Remaining asian areas in the extended EMEP domain", false},
    {Country::Id::ASM, "ASM", "Modified remaining asian areas in the former official EMEP domain", false},
    {Country::Id::AT, "AT", "Austria", true},
    {Country::Id::ATL, "ATL", "Remaining North-East Atlantic Ocean", true},
    {Country::Id::AZ, "AZ", "Azerbaijan", true},
    {Country::Id::BA, "BA", "Bosnia & Herzegovina", true},
    {Country::Id::BAS, "BAS", "Baltic Sea", true},
    {Country::Id::BEB, "BEB", "Brussels", true},
    {Country::Id::BEF, "BEF", "Flanders", true},
    {Country::Id::BEW, "BEW", "Wallonia", true},
    {Country::Id::BG, "BG", "Bulgaria", true},
    {Country::Id::BLS, "BLS", "Black Sea", true},
    {Country::Id::BY, "BY", "Belarus", true},
    {Country::Id::CAS, "CAS", "Caspian Sea", true},
    {Country::Id::CH, "CH", "Switzerland", true},
    {Country::Id::CY, "CY", "Cyprus", true},
    {Country::Id::CZ, "CZ", "Czechia", true},
    {Country::Id::DE, "DE", "Germany", true},
    {Country::Id::DK, "DK", "Denmark", true},
    {Country::Id::EE, "EE", "Estonia", true},
    {Country::Id::ES, "ES", "Spain", true},
    {Country::Id::FI, "FI", "Finland", true},
    {Country::Id::FR, "FR", "France", true},
    {Country::Id::GB, "GB", "United Kingdom", true},
    {Country::Id::GE, "GE", "Georgia", true},
    {Country::Id::GL, "GL", "Greenland", true},
    {Country::Id::GR, "GR", "Greece", true},
    {Country::Id::HR, "HR", "Croatia", true},
    {Country::Id::HU, "HU", "Hungary", true},
    {Country::Id::IE, "IE", "Ireland", true},
    {Country::Id::IS, "IS", "Iceland", true},
    {Country::Id::IT, "IT", "Italy", true},
    {Country::Id::KG, "KG", "Kyrgyzstan", false},
    {Country::Id::KZ, "KZ", "Kazakhstan", false},
    {Country::Id::KZE, "KZE", "Rest of Kazakhstan in the extended EMEP domain", false},
    {Country::Id::LI, "LI", "Liechtenstein", true},
    {Country::Id::LT, " LT", "Lithuania", true},
    {Country::Id::LU, "LU", "Luxembourg", true},
    {Country::Id::LV, "LV", "Latvia", true},
    {Country::Id::MC, "MC", "Monaco", true},
    {Country::Id::MD, "MD", "Moldova, Republic of", true},
    {Country::Id::ME, "ME", "Montenegro", true},
    {Country::Id::MED, "MED", "Mediterranean Sea", true},
    {Country::Id::MK, "MK", "North Macedonia", true},
    {Country::Id::MT, "MT", "Malta", true},
    {Country::Id::NL, "NL", "Netherlands", true},
    {Country::Id::NO, "NO", "Norway", true},
    {Country::Id::NOA, "NOA", "North Africa", false},
    {Country::Id::NOS, "NOS", "North Sea", true},
    {Country::Id::PL, "PL", "Poland", true},
    {Country::Id::PT, "PT", "Portugal", true},
    {Country::Id::RFE, "RFE", "Rest of Russion federation in the extended EMEP domain", false},
    {Country::Id::RO, "RO", "Romania", true},
    {Country::Id::RS, "RS", "Serbia", true},
    {Country::Id::RU, "RU", "Russian Federation", true},
    {Country::Id::RUX, "RUX", "EMEP-External part of Russion federation", false},
    {Country::Id::SE, "SE", "Sweden", true},
    {Country::Id::SI, "SI", "Slovenia", true},
    {Country::Id::SK, "SK", "Slovakia", true},
    {Country::Id::TJ, "TJ", "Tajikistan", false},
    {Country::Id::TME, "TME", "Rest of Turkmenistan in the extended EMEP domain", false},
    {Country::Id::TMO, "TMO", "Turkmenistan in the former official EMEP domain", false},
    {Country::Id::TR, "TR", "Turkey", true},
    {Country::Id::UA, "UA", "Ukraine", true},
    {Country::Id::UZE, "UZE", "Rest of Uzbekistan in the extended EMEP domain", false},
    {Country::Id::UZO, "UZO", "Uzbekistan in the former official EMEP domain", false},
}};

Country Country::from_string(std::string_view str)
{
    auto iter = std::find_if(s_countries.begin(), s_countries.end(), [str](const auto& pollutant) {
        return pollutant.serializedName == str;
    });

    if (iter != s_countries.end()) {
        return Country(iter->id);
    }

    throw RuntimeError("Invalid country name: '{}'", str);
}

std::optional<Country> Country::try_from_string(std::string_view str) noexcept
{
    std::optional<Country> result;

    try {
        result = Country::from_string(str);
    } catch (const std::exception&) {
    }

    return result;
}

std::string_view Country::to_string() const noexcept
{
    return code();
}

std::string_view Country::code() const noexcept
{
    return s_countries[enum_value(_id)].serializedName;
}

std::string_view Country::full_name() const noexcept
{
    return s_countries[enum_value(_id)].description;
}

bool Country::included() const noexcept
{
    return s_countries[enum_value(_id)].included;
}
}
