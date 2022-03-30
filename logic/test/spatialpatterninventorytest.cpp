#include "spatialpatterninventory.h"
#include "emap/configurationparser.h"
#include "testconfig.h"
#include "testconstants.h"
#include "testprinters.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;
using namespace date;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv, const fs::path& exceptionsPath)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = "./out";
    outputConfig.outputLevelName = "NFR";

    return RunConfiguration(fs::u8path(TEST_DATA_DIR) / "_input", exceptionsPath, ModelGrid::Vlops1km, RunType::Emep, ValidationType::NoValidation, 2016_y, 2021_y, "", {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Spatial pattern selection test")
{
    const auto parametersPath  = fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto sectorInventory = parse_sectors(parametersPath / "id_nummers.xlsx",
                                               parametersPath / "code_conversions.xlsx",
                                               parametersPath / "names_to_be_ignored.xlsx");

    auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx",
                                               parametersPath / "code_conversions.xlsx",
                                               parametersPath / "names_to_be_ignored.xlsx");

    auto countryInventory = parse_countries(parametersPath / "id_nummers.xlsx");

    pollutantInventory.add_fallback_for_pollutant(pollutantInventory.pollutant_from_string("PMcoarse"), pollutantInventory.pollutant_from_string("PM10"));

    auto exceptionsPath = fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "exceptions_spatial_disaggregation.xlsx";
    auto cfg            = create_config(sectorInventory, pollutantInventory, countryInventory, exceptionsPath);

    SpatialPatternTableCache cache(cfg);
    SpatialPatternInventory inv(cfg);
    inv.scan_dir(2021_y, 2016_y, fs::u8path(TEST_DATA_DIR) / "spatialinventory");

    {
        // Available in 2016 at gnfr level: Industry
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::CO));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_co_B_Industry.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::CO);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2016 at gnfr level: Industry
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PMcoarse));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm10_B_Industry.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2015 at gnfr level: Waste
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C1bv), pollutants::PMcoarse));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_pm10_J_Waste.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C1bv));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2015_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Public power
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::SOx));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_so2_A_PublicPower.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::SOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Industry, cams version in filename is different
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.3_2016_co_A_PublicPower.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::CO);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at nfr
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C2), pollutants::PM2_5));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm2_5_5C2.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::PM2_5);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C2));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2015 at gnfr (not in 2016)
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr3Da1), pollutants::NOx));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_nox_L_AgriOther.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr3Da1));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2015_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2017 at gnfr (not in 2016 or 2015)
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr2D3d), pollutants::NOx));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2017" / "CAMS_emissions_REG-APv5.1_2017_nox_E_Solvents.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr2D3d));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2017_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2018 at gnfr (not in 2016 or 2015 or 2017 or 2014)
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1B2b), pollutants::NOx));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2018" / "CAMS_emissions_REG-APv5.1_2018_nox_D_Fugitives.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1B2b));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2018_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2010 at gnfr (not in 2016 or 2015 or 2017 or 2014 or 2018 or ...)
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NOx));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2010" / "CAMS_emissions_REG-APv5.1_2010_nox_I_OffRoad.tif");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A4bi));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2010_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // No spatial mapping available: Use uniform spread
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NMVOC));
        CHECK(spSource.path.empty());
        CHECK(spSource.emissionId.pollutant == pollutants::NMVOC);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(spSource.type == SpatialPatternSource::Type::UnfiformSpread);
    }

    {
        // Available in 2016 at gnfr level: Public power
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::BaP));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CEIP" / "2016" / "BaP_A_PublicPower_2018_GRID_2016.txt");
        CHECK(spSource.emissionId.pollutant == pollutants::BaP);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternCEIP);
    }

    {
        // Flanders excel data
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::NOx), &cache);
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2015" / "Emissies per km2 excl puntbrongegevens_2015_NOx.xlsx");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2015_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternTable);
    }

    {
        // Flanders excel data, 2015 does not contain valid data for the sector, so 2019 needs to be used
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NOx), &cache);
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NOx.xlsx");
        CHECK(spSource.emissionId.pollutant == pollutants::NOx);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2019_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternTable);
    }

    {
        // Flanders excel data
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PM2_5), &cache);
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_PM2,5.xlsx");
        CHECK(spSource.emissionId.pollutant == pollutants::PM2_5);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2019_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternTable);
    }

    {
        // Flanders excel data
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::As), &cache);
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissie per km2_met NFR_As 2019_juli 2021.xlsx");
        CHECK(spSource.emissionId.pollutant == pollutants::As);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2019_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPatternTable);
    }

    {
        // Exception rule
        const auto spSource = inv.get_spatial_pattern(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::CO), &cache);
        CHECK(spSource.path.generic_u8string() == (fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "." / "wegverkeer" / "2021" / "1A3BI_CO_2016.tif").generic_u8string());
        CHECK(spSource.emissionId.pollutant == pollutants::CO);
        CHECK(spSource.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A3bi));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.type == SpatialPatternSource::Type::RasterException);
    }
}

}
