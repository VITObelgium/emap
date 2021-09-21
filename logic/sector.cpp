#include "emap/sector.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace emap {

using namespace inf;

static constexpr std::array<EnumInfo<GnfrSector>, enum_count<GnfrSector>()> s_gnfrSectors = {{
    {GnfrSector::PublicPower, "A_PublicPower", "Public power"},
    {GnfrSector::Industry, "B_Industry", "Industry"},
    {GnfrSector::OtherStationaryComb, "C_OtherStatComb", "Other stationary combustion"},
    {GnfrSector::Fugitive, "D_Fugitive", "Fugitive"},
    {GnfrSector::Solvents, "E_Solvents", "Solvents"},
    {GnfrSector::RoadTransport, "F_RoadTransport", "Road transport"},
    {GnfrSector::Shipping, "G_Shipping", "Shipping"},
    {GnfrSector::Aviation, "H_Aviation", "Aviation"},
    {GnfrSector::Offroad, "I_Offroad", "Offroad"},
    {GnfrSector::Waste, "J_Waste", "Waste"},
    {GnfrSector::AgriLivestock, "K_AgriLivestock", "Agriculture: live stock"},
    {GnfrSector::AgriOther, "L_AgriOther", "Agriculture: other"},
    {GnfrSector::Other, "M_Other", "Other"},
}};

static constexpr std::array<EnumInfo<NfrSector>, enum_count<NfrSector>()> s_nfrSectors = {{
    {NfrSector::Nfr1A1a, "1A1a", "Public electricity and heat production"},
    {NfrSector::Nfr1A1b, "1A1b", "Petroleum refining"},
    {NfrSector::Nfr1A1c, "1A1c", "Manufacture of solid fuels and other energy industries"},
    {NfrSector::Nfr1A2a, "1A2a", "Stationary combustion in manufacturing industries and construction: Iron and steel"},
    {NfrSector::Nfr1A2b, "1A2b", "Stationary combustion in manufacturing industries and construction: Non-ferrous metals"},
    {NfrSector::Nfr1A2c, "1A2c", "Stationary combustion in manufacturing industries and construction: Chemicals"},
    {NfrSector::Nfr1A2d, "1A2d", "Stationary combustion in manufacturing industries and construction: Pulp, Paper and Print"},
    {NfrSector::Nfr1A2e, "1A2e", "Stationary combustion in manufacturing industries and construction: Food processing, beverages and tobacco"},
    {NfrSector::Nfr1A2f, "1A2f", "Stationary combustion in manufacturing industries and construction: Non-metallic minerals"},
    {NfrSector::Nfr1A2gvii, "1A2gvii", "Mobile combustion in manufacturing industries and construction (please specify in the IIR)"},
    {NfrSector::Nfr1A2gviii, "1A2gviii", "Stationary combustion in manufacturing industries and construction: Other (please specify in the IIR)"},
    {NfrSector::Nfr1A3ai_i, "1A3ai(i)", "International aviation LTO (civil)"},
    {NfrSector::Nfr1A3aii_i, "1A3aii(i)", "Domestic aviation LTO (civil)"},
    {NfrSector::Nfr1A3bi, "1A3bi", "Road transport: Passenger cars"},
    {NfrSector::Nfr1A3bii, "1A3bii", "Road transport: Light duty vehicles"},
    {NfrSector::Nfr1A3biii, "1A3biii", "Road transport: Heavy duty vehicles and buses"},
    {NfrSector::Nfr1A3biv, "1A3biv", "Road transport: Mopeds & motorcycles"},
    {NfrSector::Nfr1A3bv, "1A3bv", "Road transport: Gasoline evaporation"},
    {NfrSector::Nfr1A3bvi, "1A3bvi", "Road transport: Automobile tyre and brake wear"},
    {NfrSector::Nfr1A3bvii, "1A3bvii", "Road transport: Automobile road abrasion"},
    {NfrSector::Nfr1A3c, "1A3c", "Railways"},
    {NfrSector::Nfr1A3di_ii, "1A3di(ii)", "International inland waterways"},
    {NfrSector::Nfr1A3dii, "1A3dii", "National navigation (shipping)"},
    {NfrSector::Nfr1A3ei, "1A3ei", "Pipeline transport"},
    {NfrSector::Nfr1A3eii, "1A3eii", "Other (please specify in the IIR)"},
    {NfrSector::Nfr1A4ai, "1A4ai", "Commercial/Institutional: Stationary"},
    {NfrSector::Nfr1A4aii, "1A4aii", "Commercial/Institutional: Mobile"},
    {NfrSector::Nfr1A4bi, "1A4bi", "Residential: Stationary"},
    {NfrSector::Nfr1A4bii, "1A4bii", "Residential: Household and gardening (mobile)"},
    {NfrSector::Nfr1A4ci, "1A4ci", "Agriculture/Forestry/Fishing: Stationary"},
    {NfrSector::Nfr1A4cii, "1A4cii", "Agriculture/Forestry/Fishing: Off-road vehicles and other machinery"},
    {NfrSector::Nfr1A4ciii, "1A4ciii", "Agriculture/Forestry/Fishing: National fishing"},
    {NfrSector::Nfr1A5a, "1A5a", "Other stationary (including military)"},
    {NfrSector::Nfr1A5b, "1A5b", "Other, Mobile (including military, land based and recreational boats)"},
    {NfrSector::Nfr1B1a, "1B1a", "Fugitive emission from solid fuels: Coal mining and handling"},
    {NfrSector::Nfr1B1b, "1B1b", "Fugitive emission from solid fuels: Solid fuel transformation"},
    {NfrSector::Nfr1B1c, "1B1c", "Other fugitive emissions from solid fuels"},
    {NfrSector::Nfr1B2ai, "1B2ai", "Fugitive emissions oil: Exploration, production, transport"},
    {NfrSector::Nfr1B2aiv, "1B2aiv", "Fugitive emissions oil: Refining and storage"},
    {NfrSector::Nfr1B2av, "1B2av", "Distribution of oil products"},
    {NfrSector::Nfr1B2b, "1B2b", "Fugitive emissions from natural gas (exploration, production, processing, transmission, storage, distribution and other)"},
    {NfrSector::Nfr1B2c, "1B2c", "Venting and flaring (oil, gas, combined oil and gas)"},
    {NfrSector::Nfr1B2d, "1B2d", "Other fugitive emissions from energy production"},
    {NfrSector::Nfr2A1, "2A1", "Cement production"},
    {NfrSector::Nfr2A2, "2A2", "Lime production"},
    {NfrSector::Nfr2A3, "2A3", "Glass production"},
    {NfrSector::Nfr2A5a, "2A5a", "Quarrying and mining of minerals other than coal"},
    {NfrSector::Nfr2A5b, "2A5b", "Construction and demolition"},
    {NfrSector::Nfr2A5c, "2A5c", "Storage, handling and transport of mineral products"},
    {NfrSector::Nfr2A6, "2A6", "Other mineral products (please specify in the IIR)"},
    {NfrSector::Nfr2B1, "2B1", "Ammonia production"},
    {NfrSector::Nfr2B2, "2B2", "Nitric acid production"},
    {NfrSector::Nfr2B3, "2B3", "Adipic acid production"},
    {NfrSector::Nfr2B5, "2B5", "Carbide production"},
    {NfrSector::Nfr2B6, "2B6", "Titanium dioxide production"},
    {NfrSector::Nfr2B7, "2B7", "Soda ash production"},
    {NfrSector::Nfr2B10a, "2B10a", "Chemical industry: Other (please specify in the IIR)"},
    {NfrSector::Nfr2B10b, "2B10b", "Storage, handling and transport of chemical products (please specify in the IIR)"},
    {NfrSector::Nfr2C1, "2C1", "Iron and steel production"},
    {NfrSector::Nfr2C2, "2C2", "Ferroalloys production"},
    {NfrSector::Nfr2C3, "2C3", "Aluminium production"},
    {NfrSector::Nfr2C4, "2C4", "Magnesium production"},
    {NfrSector::Nfr2C5, "2C5", "Lead production"},
    {NfrSector::Nfr2C6, "2C6", "Zinc production"},
    {NfrSector::Nfr2C7a, "2C7a", "Copper production"},
    {NfrSector::Nfr2C7b, "2C7b", "Nickel production"},
    {NfrSector::Nfr2C7c, "2C7c", "Other metal production (please specify in the IIR)"},
    {NfrSector::Nfr2C7d, "2C7d", "Storage, handling and transport of metal products (please specify in the IIR)"},
    {NfrSector::Nfr2D3a, "2D3a", "Domestic solvent use including fungicides"},
    {NfrSector::Nfr2D3b, "2D3b", "Road paving with asphalt"},
    {NfrSector::Nfr2D3c, "2D3c", "Asphalt roofing"},
    {NfrSector::Nfr2D3d, "2D3d", "Coating applications"},
    {NfrSector::Nfr2D3e, "2D3e", "Degreasing"},
    {NfrSector::Nfr2D3f, "2D3f", "Dry cleaning"},
    {NfrSector::Nfr2D3g, "2D3g", "Chemical products"},
    {NfrSector::Nfr2D3h, "2D3h", "Printing"},
    {NfrSector::Nfr2D3i, "2D3i", "Other solvent use (please specify in the IIR)"},
    {NfrSector::Nfr2G, "2G", "Other product use (please specify in the IIR)"},
    {NfrSector::Nfr2H1, "2H1", "Pulp and paper industry"},
    {NfrSector::Nfr2H2, "2H2", "Food and beverages industry"},
    {NfrSector::Nfr2H3, "2H3", "Other industrial processes (please specify in the IIR)"},
    {NfrSector::Nfr2I, "2I", "Wood processing"},
    {NfrSector::Nfr2J, "2J", "Production of POPs"},
    {NfrSector::Nfr2K, "2K", "Consumption of POPs and heavy metals (e.g. electrical and scientific equipment)"},
    {NfrSector::Nfr2L, "2L", "Other production, consumption, storage, transportation or handling of bulk products (please specify in the IIR)"},
    {NfrSector::Nfr3B1a, "3B1a", "Manure management - Dairy cattle"},
    {NfrSector::Nfr3B1b, "3B1b", "Manure management - Non-dairy cattle"},
    {NfrSector::Nfr3B2, "3B2", "Manure management - Sheep"},
    {NfrSector::Nfr3B3, "3B3", "Manure management - Swine"},
    {NfrSector::Nfr3B4a, "3B4a", "Manure management - Buffalo"},
    {NfrSector::Nfr3B4d, "3B4d", "Manure management - Goats"},
    {NfrSector::Nfr3B4e, "3B4e", "Manure management - Horses"},
    {NfrSector::Nfr3B4f, "3B4f", "Manure management - Mules and asses"},
    {NfrSector::Nfr3B4gi, "3B4gi", "Manure management - Laying hens"},
    {NfrSector::Nfr3B4gii, "3B4gii", "Manure management - Broilers"},
    {NfrSector::Nfr3B4giii, "3B4giii", "Manure management - Turkeys"},
    {NfrSector::Nfr3B4giv, "3B4giv", "Manure management - Other poultry"},
    {NfrSector::Nfr3B4h, "3B4h", "Manure management - Other animals (please specify in the IIR)"},
    {NfrSector::Nfr3Da1, "3Da1", "Inorganic N-fertilizers (includes also urea application)"},
    {NfrSector::Nfr3Da2a, "3Da2a", "Animal manure applied to soils"},
    {NfrSector::Nfr3Da2b, "3Da2b", "Sewage sludge applied to soils"},
    {NfrSector::Nfr3Da2c, "3Da2c", "Other organic fertilisers applied to soils (including compost)"},
    {NfrSector::Nfr3Da3, "3Da3", "Urine and dung deposited by grazing animals"},
    {NfrSector::Nfr3Da4, "3Da4", "Crop residues applied to soils"},
    {NfrSector::Nfr3Db, "3Db", "Indirect emissions from managed soils"},
    {NfrSector::Nfr3Dc, "3Dc", "Farm-level agricultural operations including storage, handling and transport of agricultural products"},
    {NfrSector::Nfr3Dd, "3Dd", "Off-farm storage, handling and transport of bulk agricultural products"},
    {NfrSector::Nfr3De, "3De", "Cultivated crops"},
    {NfrSector::Nfr3Df, "3Df", "Use of pesticides"},
    {NfrSector::Nfr3F, "3F", "Field burning of agricultural residues"},
    {NfrSector::Nfr3I, "3I", "Agriculture other (please specify in the IIR)"},
    {NfrSector::Nfr5A, "5A", "Biological treatment of waste - Solid waste disposal on land"},
    {NfrSector::Nfr5B1, "5B1", "Biological treatment of waste - Composting"},
    {NfrSector::Nfr5B2, "5B2", "Biological treatment of waste - Anaerobic digestion at biogas facilities"},
    {NfrSector::Nfr5C1a, "5C1a", "Municipal waste incineration"},
    {NfrSector::Nfr5C1bi, "5C1bi", "Industrial waste incineration"},
    {NfrSector::Nfr5C1bii, "5C1bii", "Hazardous waste incineration"},
    {NfrSector::Nfr5C1biii, "5C1biii", "Clinical waste incineration"},
    {NfrSector::Nfr5C1biv, "5C1biv", "Sewage sludge incineration"},
    {NfrSector::Nfr5C1bv, "5C1bv", "Cremation"},
    {NfrSector::Nfr5C1bvi, "5C1bvi", "Other waste incineration (please specify in the IIR)"},
    {NfrSector::Nfr5C2, "5C2", "Open burning of waste"},
    {NfrSector::Nfr5D1, "5D1", "Domestic wastewater handling"},
    {NfrSector::Nfr5D2, "5D2", "Industrial wastewater handling"},
    {NfrSector::Nfr5D3, "5D3", "Other wastewater handling"},
    {NfrSector::Nfr5E, "5E", "Other waste (please specify in the IIR)"},
    {NfrSector::Nfr6A, "6A", "Other (included in national total for entire territory) (please specify in the IIR)"},
    {NfrSector::Nfr1A3bi_fu, "1A3bi(fu)", "Road transport: Passenger cars (fuel used)"},
    {NfrSector::Nfr1A3bii_fu, "1A3bii(fu)", "Road transport: Light duty vehicles (fuel used)"},
    {NfrSector::Nfr1A3biii_fu, "1A3biii(fu)", "Road transport: Heavy duty vehicles and buses (fuel used)"},
    {NfrSector::Nfr1A3biv_fu, "1A3biv(fu)", "Road transport: Mopeds & motorcycles (fuel used)"},
    {NfrSector::Nfr1A3bv_fu, "1A3bv(fu)", "Road transport: Gasoline evaporation (fuel used)"},
    {NfrSector::Nfr1A3bvi_fu, "1A3bvi(fu)", "Road transport: Automobile tyre and brake wear (fuel used)"},
    {NfrSector::Nfr1A3bvii_fu, "1A3bvii(fu)", "Road transport: Automobile road abrasion (fuel used)"},
}};

static constexpr std::array<GnfrSector, enum_count<NfrSector>()> s_nfrToGnfrMapping = {{
    GnfrSector::PublicPower,         //1A1a
    GnfrSector::Industry,            //1A1b
    GnfrSector::Industry,            //1A1c
    GnfrSector::Industry,            //1A2a
    GnfrSector::Industry,            //1A2b
    GnfrSector::Industry,            //1A2c
    GnfrSector::Industry,            //1A2d
    GnfrSector::Industry,            //1A2e
    GnfrSector::Industry,            //1A2f
    GnfrSector::Industry,            //1A2gviii
    GnfrSector::Industry,            //2A1
    GnfrSector::Industry,            //2A2
    GnfrSector::Industry,            //2A3
    GnfrSector::Industry,            //2A5a
    GnfrSector::Industry,            //2A5b
    GnfrSector::Industry,            //2A5c
    GnfrSector::Industry,            //2A6
    GnfrSector::Industry,            //2B1
    GnfrSector::Industry,            //2B2
    GnfrSector::Industry,            //2B3
    GnfrSector::Industry,            //2B5
    GnfrSector::Industry,            //2B6
    GnfrSector::Industry,            //2B7
    GnfrSector::Industry,            //2B10a
    GnfrSector::Industry,            //2B10b
    GnfrSector::Industry,            //2C1
    GnfrSector::Industry,            //2C2
    GnfrSector::Industry,            //2C3
    GnfrSector::Industry,            //2C4
    GnfrSector::Industry,            //2C5
    GnfrSector::Industry,            //2C6
    GnfrSector::Industry,            //2C7a
    GnfrSector::Industry,            //2C7b
    GnfrSector::Industry,            //2C7c
    GnfrSector::Industry,            //2C7d
    GnfrSector::Industry,            //2D3b
    GnfrSector::Industry,            //2D3c
    GnfrSector::Industry,            //2H1
    GnfrSector::Industry,            //2H2
    GnfrSector::Industry,            //2H3
    GnfrSector::Industry,            //2I
    GnfrSector::Industry,            //2J
    GnfrSector::Industry,            //2K
    GnfrSector::Industry,            //2L
    GnfrSector::OtherStationaryComb, //1A4ai
    GnfrSector::OtherStationaryComb, //1A4bi
    GnfrSector::OtherStationaryComb, //1A4ci
    GnfrSector::OtherStationaryComb, //1A5a
    GnfrSector::Fugitive,            //1B1a
    GnfrSector::Fugitive,            //1B1b
    GnfrSector::Fugitive,            //1B1c
    GnfrSector::Fugitive,            //1B2ai
    GnfrSector::Fugitive,            //1B2aiv
    GnfrSector::Fugitive,            //1B2av
    GnfrSector::Fugitive,            //1B2b
    GnfrSector::Fugitive,            //1B2c
    GnfrSector::Fugitive,            //1B2d
    GnfrSector::Solvents,            //2D3a
    GnfrSector::Solvents,            //2D3d
    GnfrSector::Solvents,            //2D3e
    GnfrSector::Solvents,            //2D3f
    GnfrSector::Solvents,            //2D3g
    GnfrSector::Solvents,            //2D3h
    GnfrSector::Solvents,            //2D3i
    GnfrSector::Solvents,            //2G
    GnfrSector::RoadTransport,       //1A3bi
    GnfrSector::RoadTransport,       //1A3bii
    GnfrSector::RoadTransport,       //1A3biii
    GnfrSector::RoadTransport,       //1A3biv
    GnfrSector::RoadTransport,       //1A3bv
    GnfrSector::RoadTransport,       //1A3bvi
    GnfrSector::RoadTransport,       //1A3bvii
    GnfrSector::Shipping,            //1A3di(ii)
    GnfrSector::Shipping,            //1A3dii
    GnfrSector::Aviation,            //1A3ai(i)
    GnfrSector::Aviation,            //1A3aii(i)
    GnfrSector::Offroad,             //1A2gvii
    GnfrSector::Offroad,             //1A3c
    GnfrSector::Offroad,             //1A3ei
    GnfrSector::Offroad,             //1A3eii
    GnfrSector::Offroad,             //1A4aii
    GnfrSector::Offroad,             //1A4bii
    GnfrSector::Offroad,             //1A4cii
    GnfrSector::Offroad,             //1A4ciii
    GnfrSector::Offroad,             //1A5b
    GnfrSector::Waste,               //5A
    GnfrSector::Waste,               //5B1
    GnfrSector::Waste,               //5B2
    GnfrSector::Waste,               //5C1a
    GnfrSector::Waste,               //5C1bi
    GnfrSector::Waste,               //5C1bii
    GnfrSector::Waste,               //5C1biii
    GnfrSector::Waste,               //5C1biv
    GnfrSector::Waste,               //5C1bv
    GnfrSector::Waste,               //5C1bvi
    GnfrSector::Waste,               //5C2
    GnfrSector::Waste,               //5D1
    GnfrSector::Waste,               //5D2
    GnfrSector::Waste,               //5D3
    GnfrSector::Waste,               //5E
    GnfrSector::AgriLivestock,       //3B1a
    GnfrSector::AgriLivestock,       //3B1b
    GnfrSector::AgriLivestock,       //3B2
    GnfrSector::AgriLivestock,       //3B3
    GnfrSector::AgriLivestock,       //3B4a
    GnfrSector::AgriLivestock,       //3B4d
    GnfrSector::AgriLivestock,       //3B4e
    GnfrSector::AgriLivestock,       //3B4f
    GnfrSector::AgriLivestock,       //3B4gi
    GnfrSector::AgriLivestock,       //3B4gii
    GnfrSector::AgriLivestock,       //3B4giii
    GnfrSector::AgriLivestock,       //3B4giv
    GnfrSector::AgriLivestock,       //3B4h
    GnfrSector::AgriOther,           //3Da1
    GnfrSector::AgriOther,           //3Da2a
    GnfrSector::AgriOther,           //3Da2b
    GnfrSector::AgriOther,           //3Da2c
    GnfrSector::AgriOther,           //3Da3
    GnfrSector::AgriOther,           //3Da4
    GnfrSector::AgriOther,           //3Db
    GnfrSector::AgriOther,           //3Dc
    GnfrSector::AgriOther,           //3Dd
    GnfrSector::AgriOther,           //3De
    GnfrSector::AgriOther,           //3Df
    GnfrSector::AgriOther,           //3F
    GnfrSector::AgriOther,           //3I
    GnfrSector::Other,               //6A
    GnfrSector::RoadTransport,       //1A3bi(fu)
    GnfrSector::RoadTransport,       //1A3bii(fu)
    GnfrSector::RoadTransport,       //1A3biii(fu)
    GnfrSector::RoadTransport,       //1A3biv(fu)
    GnfrSector::RoadTransport,       //1A3bv(fu)
    GnfrSector::RoadTransport,       //1A3bvi(fu)
    GnfrSector::RoadTransport,       //1A3bvii(fu)
}};

constexpr std::string_view sector_name(GnfrSector sector) noexcept
{
    return s_gnfrSectors[enum_value(sector)].serializedName;
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
    if (type() == Type::Gnfr) {
        return name();
    }

    return sector_name(s_nfrToGnfrMapping[enum_value(nfr_sector())]);
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

    switch (nfr_sector()) {
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

NfrSector EmissionSector::nfr_sector() const noexcept
{
    assert(type() == Type::Nfr);
    return std::get<NfrSector>(_sector);
}

GnfrSector EmissionSector::gnfr_sector() const noexcept
{
    assert(type() == Type::Gnfr);
    return std::get<GnfrSector>(_sector);
}

template <typename SectorInfo>
static auto find_sector(std::string_view str, const SectorInfo& sectors)
{
    return std::find_if(sectors.begin(), sectors.end(), [str](const auto& sector) {
        return sector.serializedName == str;
    });
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
