#include "emap/sector.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

static const std::array<MultiEnumInfo<GnfrSector>, enum_count<GnfrSector>()> s_gnfrSectors = {{
    {GnfrSector::PublicPower, {"A_PublicPower"sv}, "Public power"},
    {GnfrSector::Industry, {"B_Industry"sv}, "Industry"},
    {GnfrSector::OtherStationaryComb, {"C_OtherStatComb"sv, "C_OtherStationaryComb"sv}, "Other stationary combustion"},
    {GnfrSector::Fugitive, {"D_Fugitive"sv, "D_Fugitives"sv}, "Fugitive"},
    {GnfrSector::Solvents, {"E_Solvents"sv}, "Solvents"},
    {GnfrSector::RoadTransport, {"F_RoadTransport"sv}, "Road transport"},
    {GnfrSector::Shipping, {"G_Shipping"sv}, "Shipping"},
    {GnfrSector::Aviation, {"H_Aviation"sv}, "Aviation"},
    {GnfrSector::Offroad, {"I_Offroad"sv, "I_OffRoad"sv}, "Offroad"},
    {GnfrSector::Waste, {"J_Waste"sv}, "Waste"},
    {GnfrSector::AgriLivestock, {"K_AgriLivestock"sv}, "Agriculture: live stock"},
    {GnfrSector::AgriOther, {"L_AgriOther"sv}, "Agriculture: other"},
    {GnfrSector::Other, {"M_Other"sv}, "Other"},
}};

struct NfrEnumInfo : public EnumInfo<NfrSector>
{
    GnfrSector gnfr;
};

static constexpr std::array<NfrEnumInfo, enum_count<NfrSector>()> s_nfrSectors = {{
    {NfrSector::Nfr1A1a, "1A1a", "Public electricity and heat production", GnfrSector::PublicPower},
    {NfrSector::Nfr1A1b, "1A1b", "Petroleum refining", GnfrSector::Industry},
    {NfrSector::Nfr1A1c, "1A1c", "Manufacture of solid fuels and other energy industries", GnfrSector::Industry},
    {NfrSector::Nfr1A2a, "1A2a", "Stationary combustion in manufacturing industries and construction: Iron and steel", GnfrSector::Industry},
    {NfrSector::Nfr1A2b, "1A2b", "Stationary combustion in manufacturing industries and construction: Non-ferrous metals", GnfrSector::Industry},
    {NfrSector::Nfr1A2c, "1A2c", "Stationary combustion in manufacturing industries and construction: Chemicals", GnfrSector::Industry},
    {NfrSector::Nfr1A2d, "1A2d", "Stationary combustion in manufacturing industries and construction: Pulp, Paper and Print", GnfrSector::Industry},
    {NfrSector::Nfr1A2e, "1A2e", "Stationary combustion in manufacturing industries and construction: Food processing, beverages and tobacco", GnfrSector::Industry},
    {NfrSector::Nfr1A2f, "1A2f", "Stationary combustion in manufacturing industries and construction: Non-metallic minerals", GnfrSector::Industry},
    {NfrSector::Nfr1A2gvii, "1A2gvii", "Mobile combustion in manufacturing industries and construction (please specify in the IIR)", GnfrSector::Offroad},
    {NfrSector::Nfr1A2gviii, "1A2gviii", "Stationary combustion in manufacturing industries and construction: Other (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr1A3ai_i, "1A3ai(i)", "International aviation LTO (civil)", GnfrSector::Aviation},
    {NfrSector::Nfr1A3aii_i, "1A3aii(i)", "Domestic aviation LTO (civil)", GnfrSector::Aviation},
    {NfrSector::Nfr1A3bi, "1A3bi", "Road transport: Passenger cars", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bii, "1A3bii", "Road transport: Light duty vehicles", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3biii, "1A3biii", "Road transport: Heavy duty vehicles and buses", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3biv, "1A3biv", "Road transport: Mopeds & motorcycles", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bv, "1A3bv", "Road transport: Gasoline evaporation", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bvi, "1A3bvi", "Road transport: Automobile tyre and brake wear", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bvii, "1A3bvii", "Road transport: Automobile road abrasion", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3c, "1A3c", "Railways", GnfrSector::Offroad},
    {NfrSector::Nfr1A3di_ii, "1A3di(ii)", "International inland waterways", GnfrSector::Shipping},
    {NfrSector::Nfr1A3dii, "1A3dii", "National navigation (shipping)", GnfrSector::Shipping},
    {NfrSector::Nfr1A3ei, "1A3ei", "Pipeline transport", GnfrSector::Offroad},
    {NfrSector::Nfr1A3eii, "1A3eii", "Other (please specify in the IIR)", GnfrSector::Offroad},
    {NfrSector::Nfr1A4ai, "1A4ai", "Commercial/Institutional: Stationary", GnfrSector::Offroad},
    {NfrSector::Nfr1A4aii, "1A4aii", "Commercial/Institutional: Mobile", GnfrSector::Offroad},
    {NfrSector::Nfr1A4bi, "1A4bi", "Residential: Stationary", GnfrSector::Offroad},
    {NfrSector::Nfr1A4bii, "1A4bii", "Residential: Household and gardening (mobile)", GnfrSector::Offroad},
    {NfrSector::Nfr1A4ci, "1A4ci", "Agriculture/Forestry/Fishing: Stationary", GnfrSector::Offroad},
    {NfrSector::Nfr1A4cii, "1A4cii", "Agriculture/Forestry/Fishing: Off-road vehicles and other machinery", GnfrSector::Offroad},
    {NfrSector::Nfr1A4ciii, "1A4ciii", "Agriculture/Forestry/Fishing: National fishing", GnfrSector::Offroad},
    {NfrSector::Nfr1A5a, "1A5a", "Other stationary (including military)", GnfrSector::Offroad},
    {NfrSector::Nfr1A5b, "1A5b", "Other, Mobile (including military, land based and recreational boats)", GnfrSector::Offroad},
    {NfrSector::Nfr1B1a, "1B1a", "Fugitive emission from solid fuels: Coal mining and handling", GnfrSector::Fugitive},
    {NfrSector::Nfr1B1b, "1B1b", "Fugitive emission from solid fuels: Solid fuel transformation", GnfrSector::Fugitive},
    {NfrSector::Nfr1B1c, "1B1c", "Other fugitive emissions from solid fuels", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2ai, "1B2ai", "Fugitive emissions oil: Exploration, production, transport", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2aiv, "1B2aiv", "Fugitive emissions oil: Refining and storage", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2av, "1B2av", "Distribution of oil products", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2b, "1B2b", "Fugitive emissions from natural gas (exploration, production, processing, transmission, storage, distribution and other)", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2c, "1B2c", "Venting and flaring (oil, gas, combined oil and gas)", GnfrSector::Fugitive},
    {NfrSector::Nfr1B2d, "1B2d", "Other fugitive emissions from energy production", GnfrSector::Fugitive},
    {NfrSector::Nfr2A1, "2A1", "Cement production", GnfrSector::Industry},
    {NfrSector::Nfr2A2, "2A2", "Lime production", GnfrSector::Industry},
    {NfrSector::Nfr2A3, "2A3", "Glass production", GnfrSector::Industry},
    {NfrSector::Nfr2A5a, "2A5a", "Quarrying and mining of minerals other than coal", GnfrSector::Industry},
    {NfrSector::Nfr2A5b, "2A5b", "Construction and demolition", GnfrSector::Industry},
    {NfrSector::Nfr2A5c, "2A5c", "Storage, handling and transport of mineral products", GnfrSector::Industry},
    {NfrSector::Nfr2A6, "2A6", "Other mineral products (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2B1, "2B1", "Ammonia production", GnfrSector::Industry},
    {NfrSector::Nfr2B2, "2B2", "Nitric acid production", GnfrSector::Industry},
    {NfrSector::Nfr2B3, "2B3", "Adipic acid production", GnfrSector::Industry},
    {NfrSector::Nfr2B5, "2B5", "Carbide production", GnfrSector::Industry},
    {NfrSector::Nfr2B6, "2B6", "Titanium dioxide production", GnfrSector::Industry},
    {NfrSector::Nfr2B7, "2B7", "Soda ash production", GnfrSector::Industry},
    {NfrSector::Nfr2B10a, "2B10a", "Chemical industry: Other (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2B10b, "2B10b", "Storage, handling and transport of chemical products (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2C1, "2C1", "Iron and steel production", GnfrSector::Industry},
    {NfrSector::Nfr2C2, "2C2", "Ferroalloys production", GnfrSector::Industry},
    {NfrSector::Nfr2C3, "2C3", "Aluminium production", GnfrSector::Industry},
    {NfrSector::Nfr2C4, "2C4", "Magnesium production", GnfrSector::Industry},
    {NfrSector::Nfr2C5, "2C5", "Lead production", GnfrSector::Industry},
    {NfrSector::Nfr2C6, "2C6", "Zinc production", GnfrSector::Industry},
    {NfrSector::Nfr2C7a, "2C7a", "Copper production", GnfrSector::Industry},
    {NfrSector::Nfr2C7b, "2C7b", "Nickel production", GnfrSector::Industry},
    {NfrSector::Nfr2C7c, "2C7c", "Other metal production (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2C7d, "2C7d", "Storage, handling and transport of metal products (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2D3a, "2D3a", "Domestic solvent use including fungicides", GnfrSector::Solvents},
    {NfrSector::Nfr2D3b, "2D3b", "Road paving with asphalt", GnfrSector::Industry},
    {NfrSector::Nfr2D3c, "2D3c", "Asphalt roofing", GnfrSector::Industry},
    {NfrSector::Nfr2D3d, "2D3d", "Coating applications", GnfrSector::Solvents},
    {NfrSector::Nfr2D3e, "2D3e", "Degreasing", GnfrSector::Solvents},
    {NfrSector::Nfr2D3f, "2D3f", "Dry cleaning", GnfrSector::Solvents},
    {NfrSector::Nfr2D3g, "2D3g", "Chemical products", GnfrSector::Solvents},
    {NfrSector::Nfr2D3h, "2D3h", "Printing", GnfrSector::Solvents},
    {NfrSector::Nfr2D3i, "2D3i", "Other solvent use (please specify in the IIR)", GnfrSector::Solvents},
    {NfrSector::Nfr2G, "2G", "Other product use (please specify in the IIR)", GnfrSector::Solvents},
    {NfrSector::Nfr2H1, "2H1", "Pulp and paper industry", GnfrSector::Industry},
    {NfrSector::Nfr2H2, "2H2", "Food and beverages industry", GnfrSector::Industry},
    {NfrSector::Nfr2H3, "2H3", "Other industrial processes (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr2I, "2I", "Wood processing", GnfrSector::Industry},
    {NfrSector::Nfr2J, "2J", "Production of POPs", GnfrSector::Industry},
    {NfrSector::Nfr2K, "2K", "Consumption of POPs and heavy metals (e.g. electrical and scientific equipment)", GnfrSector::Industry},
    {NfrSector::Nfr2L, "2L", "Other production, consumption, storage, transportation or handling of bulk products (please specify in the IIR)", GnfrSector::Industry},
    {NfrSector::Nfr3B1a, "3B1a", "Manure management - Dairy cattle", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B1b, "3B1b", "Manure management - Non-dairy cattle", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B2, "3B2", "Manure management - Sheep", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B3, "3B3", "Manure management - Swine", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4a, "3B4a", "Manure management - Buffalo", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4d, "3B4d", "Manure management - Goats", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4e, "3B4e", "Manure management - Horses", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4f, "3B4f", "Manure management - Mules and asses", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4gi, "3B4gi", "Manure management - Laying hens", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4gii, "3B4gii", "Manure management - Broilers", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4giii, "3B4giii", "Manure management - Turkeys", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4giv, "3B4giv", "Manure management - Other poultry", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3B4h, "3B4h", "Manure management - Other animals (please specify in the IIR)", GnfrSector::AgriLivestock},
    {NfrSector::Nfr3Da1, "3Da1", "Inorganic N-fertilizers (includes also urea application)", GnfrSector::AgriOther},
    {NfrSector::Nfr3Da2a, "3Da2a", "Animal manure applied to soils", GnfrSector::AgriOther},
    {NfrSector::Nfr3Da2b, "3Da2b", "Sewage sludge applied to soils", GnfrSector::AgriOther},
    {NfrSector::Nfr3Da2c, "3Da2c", "Other organic fertilisers applied to soils (including compost)", GnfrSector::AgriOther},
    {NfrSector::Nfr3Da3, "3Da3", "Urine and dung deposited by grazing animals", GnfrSector::AgriOther},
    {NfrSector::Nfr3Da4, "3Da4", "Crop residues applied to soils", GnfrSector::AgriOther},
    {NfrSector::Nfr3Db, "3Db", "Indirect emissions from managed soils", GnfrSector::AgriOther},
    {NfrSector::Nfr3Dc, "3Dc", "Farm-level agricultural operations including storage, handling and transport of agricultural products", GnfrSector::AgriOther},
    {NfrSector::Nfr3Dd, "3Dd", "Off-farm storage, handling and transport of bulk agricultural products", GnfrSector::AgriOther},
    {NfrSector::Nfr3De, "3De", "Cultivated crops", GnfrSector::AgriOther},
    {NfrSector::Nfr3Df, "3Df", "Use of pesticides", GnfrSector::AgriOther},
    {NfrSector::Nfr3F, "3F", "Field burning of agricultural residues", GnfrSector::AgriOther},
    {NfrSector::Nfr3I, "3I", "Agriculture other (please specify in the IIR)", GnfrSector::AgriOther},
    {NfrSector::Nfr5A, "5A", "Biological treatment of waste - Solid waste disposal on land", GnfrSector::Waste},
    {NfrSector::Nfr5B1, "5B1", "Biological treatment of waste - Composting", GnfrSector::Waste},
    {NfrSector::Nfr5B2, "5B2", "Biological treatment of waste - Anaerobic digestion at biogas facilities", GnfrSector::Waste},
    {NfrSector::Nfr5C1a, "5C1a", "Municipal waste incineration", GnfrSector::Waste},
    {NfrSector::Nfr5C1bi, "5C1bi", "Industrial waste incineration", GnfrSector::Waste},
    {NfrSector::Nfr5C1bii, "5C1bii", "Hazardous waste incineration", GnfrSector::Waste},
    {NfrSector::Nfr5C1biii, "5C1biii", "Clinical waste incineration", GnfrSector::Waste},
    {NfrSector::Nfr5C1biv, "5C1biv", "Sewage sludge incineration", GnfrSector::Waste},
    {NfrSector::Nfr5C1bv, "5C1bv", "Cremation", GnfrSector::Waste},
    {NfrSector::Nfr5C1bvi, "5C1bvi", "Other waste incineration (please specify in the IIR)", GnfrSector::Waste},
    {NfrSector::Nfr5C2, "5C2", "Open burning of waste", GnfrSector::Waste},
    {NfrSector::Nfr5D1, "5D1", "Domestic wastewater handling", GnfrSector::Waste},
    {NfrSector::Nfr5D2, "5D2", "Industrial wastewater handling", GnfrSector::Waste},
    {NfrSector::Nfr5D3, "5D3", "Other wastewater handling", GnfrSector::Waste},
    {NfrSector::Nfr5E, "5E", "Other waste (please specify in the IIR)", GnfrSector::Waste},
    {NfrSector::Nfr6A, "6A", "Other (included in national total for entire territory) (please specify in the IIR)", GnfrSector::Other},
    {NfrSector::Nfr1A3bi_fu, "1A3bi(fu)", "Road transport: Passenger cars (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bii_fu, "1A3bii(fu)", "Road transport: Light duty vehicles (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3biii_fu, "1A3biii(fu)", "Road transport: Heavy duty vehicles and buses (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3biv_fu, "1A3biv(fu)", "Road transport: Mopeds & motorcycles (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bv_fu, "1A3bv(fu)", "Road transport: Gasoline evaporation (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bvi_fu, "1A3bvi(fu)", "Road transport: Automobile tyre and brake wear (fuel used)", GnfrSector::RoadTransport},
    {NfrSector::Nfr1A3bvii_fu, "1A3bvii(fu)", "Road transport: Automobile road abrasion (fuel used)", GnfrSector::RoadTransport},
}};

template <typename SectorInfo>
static auto find_sector(std::string_view str, const SectorInfo& sectors)
{
    return std::find_if(sectors.begin(), sectors.end(), [str](const auto& sector) {
        return sector.is_serialized_name(str);
    });
}

std::string_view sector_name(GnfrSector sector) noexcept
{
    return s_gnfrSectors[enum_value(sector)].serialized_name();
}

constexpr std::string_view sector_name(NfrSector sector) noexcept
{
    return s_nfrSectors[enum_value(sector)].serializedName;
}

constexpr std::string_view sector_description(GnfrSector sector) noexcept
{
    return s_gnfrSectors[enum_value(sector)].description;
}

constexpr std::string_view sector_description(NfrSector sector) noexcept
{
    return s_nfrSectors[enum_value(sector)].description;
}

EmissionSector::EmissionSector(GnfrSector sector)
: _sector(sector)
{
}

EmissionSector::EmissionSector(NfrSector sector)
: _sector(sector)
{
}

EmissionSector EmissionSector::from_string(std::string_view str)
{
    if (auto iter = find_sector(str, s_nfrSectors); iter != s_nfrSectors.end()) {
        return EmissionSector(iter->id);
    }

    if (auto iter = find_sector(str, s_gnfrSectors); iter != s_gnfrSectors.end()) {
        return EmissionSector(iter->id);
    }

    throw RuntimeError("Invalid sector name: '{}'", str);
}

EmissionSector::Type EmissionSector::type() const
{
    if (std::holds_alternative<GnfrSector>(_sector)) {
        return Type::Gnfr;
    }

    if (std::holds_alternative<NfrSector>(_sector)) {
        return Type::Nfr;
    }

    assert(false);
    throw std::logic_error("Sector not properly initialized");
}

std::string_view EmissionSector::name() const noexcept
{
    assert(!_sector.valueless_by_exception());

    if (_sector.valueless_by_exception()) {
        return "unknown";
    }

    return std::visit([](auto& sectorType) {
        return sector_name(sectorType);
    },
                      _sector);
}

std::string_view EmissionSector::description() const noexcept
{
    assert(!_sector.valueless_by_exception());

    if (_sector.valueless_by_exception()) {
        return "unknown";
    }

    return std::visit([](auto& sectorType) {
        return sector_description(sectorType);
    },
                      _sector);
}

std::string_view EmissionSector::gnfr_name() const noexcept
{
    return sector_name(gnfr_sector());
}

GnfrSector EmissionSector::gnfr_sector() const noexcept
{
    if (type() == Type::Gnfr) {
        return get_gnfr_sector();
    }

    return s_nfrSectors[enum_value(get_nfr_sector())].gnfr;
}

bool EmissionSector::is_land_sector() const noexcept
{
    return gnfr_sector() != GnfrSector::Shipping;
}

std::optional<NfrSector> EmissionSector::is_sector_override() const noexcept
{
    if (type() == Type::Gnfr) {
        return {};
    }

    switch (get_nfr_sector()) {
    case NfrSector::Nfr1A3bi_fu:
        return NfrSector::Nfr1A3bi;
    case NfrSector::Nfr1A3bii_fu:
        return NfrSector::Nfr1A3bii;
    case NfrSector::Nfr1A3biii_fu:
        return NfrSector::Nfr1A3biii;
    case NfrSector::Nfr1A3biv_fu:
        return NfrSector::Nfr1A3biv;
    case NfrSector::Nfr1A3bv_fu:
        return NfrSector::Nfr1A3bv;
    case NfrSector::Nfr1A3bvi_fu:
        return NfrSector::Nfr1A3bvi;
    case NfrSector::Nfr1A3bvii_fu:
        return NfrSector::Nfr1A3bvii;
    }

    return {};
}

NfrSector EmissionSector::get_nfr_sector() const noexcept
{
    assert(type() == Type::Nfr);
    return std::get<NfrSector>(_sector);
}

GnfrSector EmissionSector::get_gnfr_sector() const noexcept
{
    assert(type() == Type::Gnfr);
    return std::get<GnfrSector>(_sector);
}

GnfrSector gnfr_sector_from_string(std::string_view str)
{
    if (auto iter = find_sector(str, s_gnfrSectors); iter != s_gnfrSectors.end()) {
        return iter->id;
    }

    throw RuntimeError("Invalid gnfr sector name: '{}'", str);
}

NfrSector nfr_sector_from_string(std::string_view str)
{
    if (auto iter = find_sector(str, s_nfrSectors); iter != s_nfrSectors.end()) {
        return iter->id;
    }

    throw RuntimeError("Invalid nfr sector name: '{}'", str);
}

}
