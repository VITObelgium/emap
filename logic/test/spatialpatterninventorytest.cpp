#include "spatialpatterninventory.h"
#include "emap/configurationparser.h"
#include "emap/countryborders.h"
#include "emap/gridprocessing.h"
#include "gdx/algo/sum.h"
#include "gdx/denserasterio.h"
#include "infra/algo.h"
#include "infra/crs.h"
#include "infra/tempdir.h"
#include "testconfig.h"
#include "testconstants.h"
#include "testprinters.h"

#include <doctest/doctest.h>
#include <limits>

namespace emap::test {

using namespace inf;
using namespace doctest;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv, const fs::path& exceptionsPath)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = "./out";
    outputConfig.outputLevelName = "NFR";

    return RunConfiguration(file::u8path(TEST_DATA_DIR) / "_input", exceptionsPath, fs::path(), fs::path(), fs::path(), ModelGrid::Vlops1km, ValidationType::NoValidation, chrono::year(2016), chrono::year(2021), "", true, 100.0, {}, sectorInv, pollutantInv, countryInv, outputConfig, std::nullopt, crs::epsg::BelgianLambert72);
}

TEST_CASE("Spatial pattern selection test")
{
    const auto parametersPath = file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    CountryInventory countryInventory(std::vector<Country>({countries::NL, countries::BEF}));
    const auto sectorInventory = parse_sectors(parametersPath / "id_nummers.xlsx",
                                               parametersPath / "code_conversions.xlsx",
                                               parametersPath / "names_to_be_ignored.xlsx",
                                               countryInventory);

    auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx",
                                               parametersPath / "code_conversions.xlsx",
                                               parametersPath / "names_to_be_ignored.xlsx",
                                               countryInventory);

    pollutantInventory.add_fallback_for_pollutant(pollutantInventory.pollutant_from_string("PMcoarse"), pollutantInventory.pollutant_from_string("PM10"));

    auto exceptionsPath = file::u8path(TEST_DATA_DIR) / "spatialinventory" / "exceptions_spatial_disaggregation.xlsx";
    auto cfg            = create_config(sectorInventory, pollutantInventory, countryInventory, exceptionsPath);

    auto grid60km = grid_data(GridDefinition::Vlops60km);
    auto grid1km  = grid_data(GridDefinition::Vlops1km);
    CountryBorders borders60km(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg", "Code3", grid60km.meta, countryInventory);
    CountryBorders borders1km(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg", "Code3", grid1km.meta, countryInventory);
    const auto coverage60km = borders60km.create_country_coverages(grid60km.meta, CoverageMode::AllCountryCells, nullptr);
    const auto coverage1km  = borders1km.create_country_coverages(grid1km.meta, CoverageMode::AllCountryCells, nullptr);

    SpatialPatternInventory inv(cfg);
    inv.scan_dir(chrono::year(2021), chrono::year(2016), file::u8path(TEST_DATA_DIR) / "spatialinventory");

    const auto& nlCoverage  = inf::find_in_container_required(coverage60km, [](auto& cov) { return cov.country == countries::NL; });
    const auto& befCoverage = inf::find_in_container_required(coverage1km, [](auto& cov) { return cov.country == countries::BEF; });

    {
        // Available in 2016 at gnfr level: Industry
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::CO), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_co_B_Industry.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Industry));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2016 at gnfr level: Industry
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PMcoarse), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm10_B_Industry.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Industry));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Fallback pollutant Available in 2015 at gnfr level: Waste
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C1bv), pollutants::PMcoarse), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_pm10_J_Waste.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PMcoarse);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C1bv));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Waste));
        CHECK(sp.source.year == chrono::year(2015));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Public power
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::SOx), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_so2_A_PublicPower.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::SOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at gnfr level: Industry, cams version in filename is different
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.3_2016_co_A_PublicPower.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2016 at nfr
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5C2), pollutants::PM2_5), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm2_5_5C2.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::PM2_5);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr5C2));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr5C2));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2015 at gnfr (not in 2016)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr3Da1), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2015" / "CAMS_emissions_REG-APv5.1_2015_nox_L_AgriOther.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr3Da1));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::AgriOther));
        CHECK(sp.source.year == chrono::year(2015));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2017 at gnfr (not in 2016 or 2015)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr2D3d), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2017" / "CAMS_emissions_REG-APv5.1_2017_nox_E_Solvents.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr2D3d));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Solvents));
        CHECK(sp.source.year == chrono::year(2017));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2018 at gnfr (not in 2016 or 2015 or 2017 or 2014)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1B2b), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2018" / "CAMS_emissions_REG-ANTv6.1_2018_nox_D_Fugitives.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1B2b));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Fugitive));
        CHECK(sp.source.year == chrono::year(2018));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
    }

    {
        // Available in 2010 at gnfr (not in 2016 or 2015 or 2017 or 2014 or 2018 or ...)
        const auto sp = inv.get_spatial_pattern(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NOx), nlCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CAMS" / "2010" / "CAMS_emissions_REG-APv5.1_2010_nox_I_OffRoad.tif");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A4bi));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Offroad));
        CHECK(sp.source.year == chrono::year(2010));
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
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "rest" / "reporting_2021" / "CEIP" / "2016" / "BaP_A_PublicPower_2018_GRID_2016.txt");
        CHECK(sp.source.emissionId.pollutant == pollutants::BaP);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::PublicPower));
        CHECK(sp.source.year == chrono::year(2016));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternCEIP);
    }

    {
        // Flanders excel data
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::NOx), befCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2015" / "Emissies per km2 excl puntbrongegevens_2015_NOx.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == chrono::year(2015));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Flanders excel data, 2015 does not contain valid data for the sector, so 2019 needs to be used
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NOx), befCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NOx.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::NOx);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A1a));
        CHECK(sp.source.year == chrono::year(2019));
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
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_PM2,5.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::PM2_5);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == chrono::year(2019));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Flanders excel data
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::As), befCoverage);
        CHECK(sp.source.path == file::u8path(TEST_DATA_DIR) / "spatialinventory" / "bef" / "reporting_2021" / "2019" / "Emissie per km2_met NFR_As 2019_juli 2021.xlsx");
        CHECK(sp.source.emissionId.pollutant == pollutants::As);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A2a));
        CHECK(sp.source.year == chrono::year(2019));
        CHECK(sp.source.type == SpatialPatternSource::Type::SpatialPatternFlanders);
        CHECK(sp.source.isException == false);
    }

    {
        // Exception rule NFR level
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::CO), befCoverage);
        CHECK(sp.source.path.generic_u8string() == (file::u8path(TEST_DATA_DIR) / "spatialinventory" / "." / "wegverkeer" / "2021" / "1A3BI_CO_2016.tif").generic_u8string());
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A3bi));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::nfr::Nfr1A3bi));
        CHECK(sp.source.type == SpatialPatternSource::Type::Raster);
        CHECK(sp.source.isException);
    }

    {
        // Exception rule GNFR level
        const auto sp = inv.get_spatial_pattern_checked(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A2c), pollutants::CO), befCoverage);
        CHECK(sp.source.path.generic_u8string() == (file::u8path(TEST_DATA_DIR) / "spatialinventory" / "." / "wegverkeer" / "2021" / "B_Industry_CO_2019.tif").generic_u8string());
        CHECK(sp.source.emissionId.pollutant == pollutants::CO);
        CHECK(sp.source.emissionId.sector == EmissionSector(sectors::nfr::Nfr1A2c));
        CHECK(sp.source.usedEmissionId.sector == EmissionSector(sectors::gnfr::Industry));
        CHECK(sp.source.type == SpatialPatternSource::Type::Raster);
        CHECK(sp.source.isException);
    }
}

TEST_CASE("Checked spatial pattern search tries regional alternatives before uniform spread")
{
    TempDir temp("spatial_pattern_alternatives");
    const CountryInventory countryInventory({countries::NL});
    InputConversions gnfrConversions;
    gnfrConversions.add_conversion("A", "A_PublicPower", std::nullopt);
    const SectorInventory sectorInventory({sectors::gnfr::PublicPower}, {sectors::nfr::Nfr1A1a}, gnfrConversions, {}, {}, {});
    PollutantInventory pollutantInventory({pollutants::PMcoarse, pollutants::PM10, pollutants::NOx}, {}, {});
    pollutantInventory.add_fallback_for_pollutant(pollutants::PMcoarse, pollutants::PM10);
    const auto cfg = create_config(sectorInventory, pollutantInventory, countryInventory, {});
    const EmissionSector nfr(sectors::nfr::Nfr1A1a);
    const EmissionSector gnfr(sectors::gnfr::PublicPower);
    const EmissionIdentifier requested(countries::NL, nfr, pollutants::PMcoarse);

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    GeoMetadata meta(1, 3, 4.0, 52.0, 0.1, nan);
    meta.set_projection_from_epsg(crs::epsg::WGS84);
    // The third cell is outside the country: positive values there must not validate a pattern.
    const CountryCellCoverage coverage{countries::NL, meta, {{Cell(0, 0), Cell(0, 0), 1.0}, {Cell(0, 1), Cell(0, 1), 1.0}}};
    const std::vector<double> valid{2.0, 6.0, 100.0};
    const std::vector<double> emptyRegion{0.0, 0.0, 100.0};
    const auto restPath = temp.path() / "rest" / "reporting_2021";
    fs::create_directories(restPath / "CAMS");
    fs::create_directories(restPath / "CEIP");

    const auto writeCams = [&](int year, std::string_view sector, const std::vector<double>& values, std::string_view pollutant = "PMcoarse") {
        const auto dir = restPath / "CAMS" / std::to_string(year);
        fs::create_directories(dir);
        const auto path = dir / fmt::format("CAMS_emissions_REG-APv5.1_{}_{}_{}.tif", year, pollutant, sector);
        gdx::write_raster(gdx::DenseRaster<double>(meta, values), path);
        return path;
    };
    const auto checkPattern = [&](const SpatialPattern& pattern, const fs::path& path, int year, const EmissionSector& usedSector, const Pollutant& usedPollutant, SpatialPatternSource::Type type = SpatialPatternSource::Type::SpatialPatternCAMS) {
        CHECK(pattern.source.path == path);
        CHECK(pattern.source.year == chrono::year(year));
        CHECK(pattern.source.type == type);
        CHECK(pattern.source.emissionId == requested);
        CHECK(pattern.source.usedEmissionId == EmissionIdentifier(countries::NL, usedSector, usedPollutant));
        CHECK_FALSE(pattern.source.isException);
        CHECK_FALSE(pattern.source.patternAvailableButWithoutData);
        REQUIRE_FALSE(pattern.raster.empty());
        CHECK(pattern.raster.metadata() == meta);
        CHECK_FALSE(pattern.raster.is_nodata(Cell(0, 0)));
        CHECK_FALSE(pattern.raster.is_nodata(Cell(0, 1)));
        CHECK(pattern.raster[Cell(0, 0)] == Approx(0.25));
        CHECK(pattern.raster[Cell(0, 1)] == Approx(0.75));
        CHECK(pattern.raster.is_nodata(Cell(0, 2)));
        CHECK(gdx::sum(pattern.raster) == Approx(1.0));
    };

    SUBCASE("Zero, nodata and data outside the country do not hide same-year GNFR")
    {
        const auto expected = writeCams(2016, "A_PublicPower", valid);
        writeCams(2015, "1A1a", valid);
        writeCams(2016, "1A1a", valid, "PM10");
        const std::vector<std::vector<double>> emptyPatterns{{0.0, 0.0, 0.0}, {nan, nan, nan}, emptyRegion};
        for (size_t i = 0; i < emptyPatterns.size(); ++i) {
            CAPTURE(i);
            const auto preferred = writeCams(2016, "1A1a", emptyPatterns[i]);
            SpatialPatternInventory inv(cfg);
            inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
            checkPattern(inv.get_spatial_pattern_checked(requested, coverage), expected, 2016, gnfr, pollutants::PMcoarse);

            const auto unchecked = inv.get_spatial_pattern(requested, coverage);
            CHECK(unchecked.source.path == preferred);
            CHECK(unchecked.source.year == chrono::year(2016));
            CHECK(unchecked.source.type == SpatialPatternSource::Type::SpatialPatternCAMS);
            CHECK(unchecked.source.emissionId == requested);
            CHECK(unchecked.source.usedEmissionId == requested);
            CHECK_FALSE(unchecked.source.isException);
            CHECK_FALSE(unchecked.source.patternAvailableButWithoutData);
            CHECK_FALSE(unchecked.raster.empty());
        }
    }

    SUBCASE("A valid exact-sector pattern still wins over GNFR")
    {
        writeCams(2016, "A_PublicPower", valid);
        const auto expected = writeCams(2016, "1A1a", valid);
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        checkPattern(inv.get_spatial_pattern_checked(requested, coverage), expected, 2016, nfr, pollutants::PMcoarse);
    }

    SUBCASE("An empty CAMS candidate does not hide same-sector CEIP")
    {
        std::string sector = "1A1a";
        auto usedSector    = nfr;
        SUBCASE("Exact-sector CEIP precedes even valid GNFR CAMS")
        {
            writeCams(2016, "A_PublicPower", valid);
        }
        SUBCASE("GNFR CEIP is tried after empty GNFR CAMS")
        {
            sector     = "A_PublicPower";
            usedSector = gnfr;
        }
        const auto preferred = writeCams(2016, sector, emptyRegion);
        const auto dir       = restPath / "CEIP" / "2016";
        fs::create_directories(dir);
        const auto expected = dir / fmt::format("PMcoarse_{}_2021_GRID_2016.txt", sector);
        file::write_as_text(expected, fmt::format("NL;2016;{};PMcoarse;4.05;52.05;Gg;2\nNL;2016;{};PMcoarse;4.15;52.05;Gg;6\n", sector, sector));
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        checkPattern(inv.get_spatial_pattern_checked(requested, coverage), expected, 2016, usedSector, pollutants::PMcoarse, SpatialPatternSource::Type::SpatialPatternCEIP);
        CHECK(inv.get_spatial_pattern(requested, coverage).source.path == preferred);
    }

    SUBCASE("Empty years are skipped without changing nearest-year priority")
    {
        writeCams(2016, "1A1a", emptyRegion);
        writeCams(2016, "A_PublicPower", emptyRegion);
        writeCams(2014, "1A1a", valid);
        writeCams(2017, "1A1a", emptyRegion);
        const auto later = writeCams(2017, "A_PublicPower", valid);
        fs::path expected;
        int expectedYear = 2015;
        SUBCASE("Equally near earlier GNFR beats later exact-sector data")
        {
            expected = writeCams(2015, "A_PublicPower", valid);
            writeCams(2017, "1A1a", valid);
        }
        SUBCASE("Later same-year GNFR alternative beats a more distant earlier year")
        {
            writeCams(2015, "1A1a", emptyRegion);
            writeCams(2015, "A_PublicPower", emptyRegion);
            expected     = later;
            expectedYear = 2017;
        }
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        checkPattern(inv.get_spatial_pattern_checked(requested, coverage), expected, expectedYear, gnfr, pollutants::PMcoarse);
    }

    SUBCASE("Pollutant fallback follows the full requested-pollutant search")
    {
        writeCams(2016, "1A1a", emptyRegion);
        writeCams(2016, "A_PublicPower", emptyRegion);
        writeCams(2017, "1A1a", emptyRegion);
        writeCams(2016, "1A1a", emptyRegion, "PM10");
        auto expected      = writeCams(2016, "A_PublicPower", valid, "PM10");
        auto usedPollutant = pollutants::PM10;
        int expectedYear   = 2016;
        SUBCASE("Fallback pollutant also tries GNFR after empty NFR")
        {
            writeCams(2017, "A_PublicPower", emptyRegion);
        }
        SUBCASE("Requested pollutant in a later year beats same-year fallback pollutant")
        {
            expected      = writeCams(2017, "A_PublicPower", valid);
            usedPollutant = pollutants::PMcoarse;
            expectedYear  = 2017;
        }
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        checkPattern(inv.get_spatial_pattern_checked(requested, coverage), expected, expectedYear, gnfr, usedPollutant);
    }

    SUBCASE("Finer grids reuse the checked alternative instead of the rejected first match")
    {
        const auto preferred = writeCams(2016, "1A1a", emptyRegion);
        const auto expected  = writeCams(2016, "A_PublicPower", valid);
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        const auto selected = inv.get_spatial_pattern_checked(requested, coverage);
        checkPattern(selected, expected, 2016, gnfr, pollutants::PMcoarse);

        GeoMetadata fineMeta(2, 4, 4.0, 52.0, 0.05, nan);
        fineMeta.set_projection_from_epsg(crs::epsg::WGS84);
        CountryCellCoverage fineCoverage{countries::NL, fineMeta, {}};
        for (int32_t row = 0; row < fineMeta.rows; ++row) {
            for (int32_t col = 0; col < fineMeta.cols; ++col) {
                fineCoverage.cells.emplace_back(Cell(row, col), Cell(row, col), 1.0);
            }
        }

        CHECK(inv.get_spatial_pattern(requested, fineCoverage).source.path == preferred);
        const auto reloaded = inv.get_spatial_pattern_for_source(selected.source, fineCoverage);
        CHECK(reloaded.source.path == expected);
        CHECK(reloaded.source.emissionId == requested);
        CHECK(reloaded.source.usedEmissionId == selected.source.usedEmissionId);
        REQUIRE_FALSE(reloaded.raster.empty());
        CHECK(reloaded.raster.metadata() == fineMeta);
        CHECK(gdx::sum(reloaded.raster) == Approx(1.0));
        CHECK(reloaded.raster[Cell(0, 0)] == Approx(0.0625));
        CHECK(reloaded.raster[Cell(0, 2)] == Approx(0.1875));
    }

    SUBCASE("Uniform spread distinguishes exhausted empty patterns from no matching pattern")
    {
        bool hadEmptyPattern = false;
        SUBCASE("Every candidate is regionally empty")
        {
            for (const auto year : {2015, 2016, 2017}) {
                writeCams(year, "1A1a", emptyRegion);
                writeCams(year, "A_PublicPower", {nan, nan, nan});
                writeCams(year, "1A1a", {0.0, 0.0, 0.0}, "PM10");
                writeCams(year, "A_PublicPower", emptyRegion, "PM10");
            }
            hadEmptyPattern = true;
        }
        SUBCASE("Only an unrelated pollutant has data")
        {
            writeCams(2016, "1A1a", valid, "NOx");
        }
        SpatialPatternInventory inv(cfg);
        inv.scan_dir(chrono::year(2021), chrono::year(2016), temp.path());
        const auto pattern = inv.get_spatial_pattern_checked(requested, coverage);
        CHECK(pattern.source.type == SpatialPatternSource::Type::UnfiformSpread);
        CHECK(pattern.source.path.empty());
        CHECK_FALSE(pattern.source.year.has_value());
        CHECK(pattern.source.emissionId == requested);
        CHECK(pattern.source.usedEmissionId == requested);
        CHECK_FALSE(pattern.source.isException);
        CHECK(pattern.source.patternAvailableButWithoutData == hadEmptyPattern);
        CHECK(pattern.raster.empty());

        const auto reloaded = inv.get_spatial_pattern_for_source(pattern.source, coverage);
        CHECK(reloaded.source.type == SpatialPatternSource::Type::UnfiformSpread);
        CHECK(reloaded.source.emissionId == requested);
        CHECK(reloaded.source.patternAvailableButWithoutData == hadEmptyPattern);
        CHECK(reloaded.raster.empty());
    }
}
}
