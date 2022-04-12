#include "spatialpatterninventory.h"
#include "emap/configurationparser.h"
#include "emap/countryborders.h"
#include "emap/gridprocessing.h"
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

    CountryInventory countryInventory(std::vector<Country>({countries::NL, countries::BEF}));

    pollutantInventory.add_fallback_for_pollutant(pollutantInventory.pollutant_from_string("PMcoarse"), pollutantInventory.pollutant_from_string("PM10"));

    auto exceptionsPath = fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "exceptions_spatial_disaggregation.xlsx";
    auto cfg            = create_config(sectorInventory, pollutantInventory, countryInventory, exceptionsPath);

    auto grid = grid_data(GridDefinition::Vlops60km);
    CountryBorders borders(fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg", "Code3", grid.meta, countryInventory);
    const auto coverage = borders.create_country_coverages(grid.meta, CoverageMode::AllCountryCells, nullptr);

    SpatialPatternInventory inv(cfg);
    inv.scan_dir(2021_y, 2016_y, fs::u8path(TEST_DATA_DIR) / "spatialinventory");

    const auto& nlCoverage  = inf::find_in_container_required(coverage, [](auto& cov) { return cov.country == countries::NL; });
    const auto& befCoverage = inf::find_in_container_required(coverage, [](auto& cov) { return cov.country == countries::BEF; });

    {
        // Available in 2016 at gnfr level: Industry
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::CO), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_co_B_Industry.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Industry));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2016 at gnfr level: Industry
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PMcoarse), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm10_B_Industry.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Industry));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2015 at gnfr level: Waste
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C1bv), pollutants::PMcoarse), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_pm10_J_Waste.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C1bv));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Waste));
        CHECK(sp.source.year == 2015_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Public power
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::SOx), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_so2_A_PublicPower.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::SOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Industry, cams version in filename is different
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.3_2016_co_A_PublicPower.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at nfr
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C2), pollutants::PM2_5), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm2_5_5C2.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PM2_5);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C2));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr5C2));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2015 at gnfr (not in 2016)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr3Da1), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_nox_L_AgriOther.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr3Da1));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::AgriOther));
        CHECK(sp.source.year == 2015_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2017 at gnfr (not in 2016 or 2015)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr2D3d), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2017" / "CAMS_emissions_REG-APv5.1_2017_nox_E_Solvents.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr2D3d));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Solvents));
        CHECK(sp.source.year == 2017_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2018 at gnfr (not in 2016 or 2015 or 2017 or 2014)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1B2b), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2018" / "CAMS_emissions_REG-APv5.1_2018_nox_D_Fugitives.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1B2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Fugitive));
        CHECK(sp.source.year == 2018_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2010 at gnfr (not in 2016 or 2015 or 2017 or 2014 or 2018 or ...)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2010" / "CAMS_emissions_REG-APv5.1_2010_nox_I_OffRoad.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A4bi));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Offroad));
        CHECK(sp.source.year == 2010_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // No spatial mapping available: Use uniform spread
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NMVOC), nlCoverage);
        CHECK(sp.source.path.empty());
        CHECK(sp.source.emissionId.pollutant == pollutants::NMVOC);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.type == SpatialPatternSource::Type::UnfiformSpread);
    }

    {
        // Available in 2016 at gnfr level: Public power
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::BaP), nlCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CEIP" / "2016" / "BaP_A_PublicPower_2018_GRID_2016.txt");
        CHECK(sp.source.emissionId.pollutant == pollutants::BaP);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == 2016_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCEIP);
    }

    {
        // Flanders excel data
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::NOx), befCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2015" / "Emissies per km2 excl puntbrongegevens_2015_NOx.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == 2015_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Flanders excel data, 2015 does not contain valid data for the sector, so 2019 needs to be used
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NOx), befCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NOx.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.year == 2019_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
        CHECK(sp.source.patternAvailableButWithoutData == false); // should only be true when we fallback to uniform spread
    }

    {
        // Flanders excel data, none of the existing patterns contain data, uniform spread will be used
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2c), pollutants::NOx), befCoverage);
        CHECK(sp.source.path.empty());
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2c));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2c));
        CHECK(sp.source.type == SpatialPatternSource::Type::UnfiformSpread);
        CHECK(sp.source.isException == false);
        CHECK(sp.source.patternAvailableButWithoutData == true);
    }

    {
        // Flanders excel data (, in pollutant name)
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PM2_5), befCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_PM2,5.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::PM2_5);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == 2019_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Flanders excel data
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::As), befCoverage);
        CHECK(sp.source.path == fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissie per km2_met NFR_As 2019_juli 2021.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::As);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == 2019_y);
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Exception rule
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::CO), befCoverage);
        CHECK(sp.source.path.generic_u8string() == (fs::u8path(TEST_DATA_DIR) / "spatialinventory" / "." / "wegverkeer" / "2021" / "1A3BI_CO_2016.tif").generic_u8string());
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A3bi));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A3bi));
        CHECK(sp.source.type == SpatialPatternSource::Type::Raster);
        CHECK(sp.source.isException);
    }
}

}
