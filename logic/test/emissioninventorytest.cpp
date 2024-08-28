#include "emap/emissioninventory.h"

#include "emap/configurationparser.h"
#include "emap/scalingfactors.h"

#include "infra/tempdir.h"
#include "runsummary.h"
#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;
using namespace date::literals;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = "./out";
    outputConfig.outputLevelName = "GNFR";

    return RunConfiguration("./data", {}, {}, ModelGrid::ChimereCams, ValidationType::NoValidation, 2016_y, 2021_y, "test", 90.0, {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

static void create_empty_point_source_file(const fs::path& path)
{
    const auto csvHeader = "type;scenario;year;reporting_country;nfr_sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;debiet_Nm3/u;dv;type_emissie;EIL_nummer;exploitatie_naam;NACE_code;EIL_Emissiepunt_Jaar_Naam;Activiteit_type";
    file::write_as_text(path, csvHeader);
}

TEST_CASE("Emission inventory")
{
    RunSummary summary;

    const auto parametersPath     = file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    auto cfg                      = create_config(sectorInventory, pollutantInventory, countryInventory);

    auto checkEmission([](const EmissionInventory& inventory, EmissionIdentifier id, double expectedDiffuse, double expectedPoint) {
        const auto emissions = inventory.emissions_with_id(id);
        REQUIRE(emissions.size() == 1);
        CHECK_MESSAGE(emissions.front().scaled_diffuse_emissions_sum() == expectedDiffuse, fmt::format("{}", id));
        CHECK_MESSAGE(emissions.front().scaled_point_emissions_sum() == expectedPoint, fmt::format("{}", id));
        CHECK_MESSAGE(emissions.front().scaled_total_emissions_sum() == expectedDiffuse + expectedPoint, fmt::format("{}", id));
    });

    auto checkNoEmission([](const EmissionInventory& inventory, EmissionIdentifier id) {
        CHECK(inventory.emissions_with_id(id).empty());
    });

    SUBCASE("Subtract point sources in Belgium")
    {
        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::ES, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(222.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(100.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(200.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), EmissionValue(500.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr2C7d), pollutants::CO), EmissionValue(300.0)));

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::FR, EmissionSector(sectors::gnfr::RoadTransport), pollutants::NOx), EmissionValue(111.0)));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::ES, EmissionSector(sectors::gnfr::RoadTransport), pollutants::NOx), EmissionValue(200.0)));

        // point emissions outside of belgium should not be used
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(11.0), Coordinate(10, 10)));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::ES, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(22.0), Coordinate(11, 11)));

        // 2 point emissions for Road NOx
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(5.0), Coordinate(100, 100)));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(7.0), Coordinate(101, 102)));

        // 1 point emissions for Road PM10
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(100.5), Coordinate(102, 103)));

        // 1 point emissions for Road PMcoarse in Brussels
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), EmissionValue(50.0), Coordinate(102, 103)));

        // no point emissions for Industry CO

        ScalingFactors scalings;
        scalings.add_scaling_factor(ScalingFactor(countries::FR, sectors::nfr::Nfr1A3bi, sectors::gnfr::RoadTransport, pollutants::NOx, EmissionSourceType::Diffuse, 2019_y, 0.5));
        scalings.add_scaling_factor(ScalingFactor(countries::BEB, sectors::nfr::Nfr1A3bi, sectors::gnfr::RoadTransport, pollutants::PMcoarse, EmissionSourceType::Point, 2019_y, 2.0));

        const auto inv = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary);

        checkEmission(inv, EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 55.5, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::ES, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 200.0, 0.0); // correction factor should be applied

        checkEmission(inv, EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 88.0, 12.0 /*5 + 7*/);
        checkEmission(inv, EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), 99.5, 100.5);
        checkEmission(inv, EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), 450, 100.0);
        checkEmission(inv, EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr2C7d), pollutants::CO), 300.0, 0.0);
    }

    SUBCASE("Spread GNFR emissions when no NFR data is available")
    {
        // empty NFR emissions
        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        ScalingFactors scalings;

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::FR, EmissionSector(sectors::gnfr::Shipping), pollutants::PM10), EmissionValue(100.0)));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::ATL, EmissionSector(sectors::gnfr::Shipping), pollutants::PM10), EmissionValue(100.0)));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::NL, EmissionSector(sectors::gnfr::RoadTransport), pollutants::PM10), EmissionValue(70.0)));

        const auto inv = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary);

        // The emission value (100.0) should be spread evenly over the two shipping sectors with type sea for seas
        checkEmission(inv, EmissionIdentifier(countries::ATL, EmissionSector(sectors::nfr::Nfr1A3di_i), pollutants::PM10), 100.0, 0.0);
        checkNoEmission(inv, EmissionIdentifier(countries::ATL, EmissionSector(sectors::nfr::Nfr1A3di_ii), pollutants::PM10));
        checkNoEmission(inv, EmissionIdentifier(countries::ATL, EmissionSector(sectors::nfr::Nfr1A3dii), pollutants::PM10));

        // The emission value (100.0) should be spread evenly over the two shipping sectors with type land for countries
        checkNoEmission(inv, EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3di_i), pollutants::PM10));
        checkEmission(inv, EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3di_ii), pollutants::PM10), 50.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3dii), pollutants::PM10), 50.0, 0.0);

        // Exclude resuspension from Road transport
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bii), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3biii), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bvi), pollutants::PM10), 10.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bvii), pollutants::PM10), 10.0, 0.0);
        checkNoEmission(inv, EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A3bviii), pollutants::PM10));
    }

    SUBCASE("Correct NFR values based on GNFR")
    {
        ScalingFactors scalings;

        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        // all aviation sectors have a value
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::NOx), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::NOx), EmissionValue(222.0)));
        // 2 out of 7 offroad sectors have a value
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::NOx), EmissionValue(50.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NOx), EmissionValue(60.0)));
        // pollutant emissions should not corrected to 0.0 when GNFR is missing (GNFR is considered to be a validated 0.0)
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::Cd), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::Cd), EmissionValue(222.0)));
        // TSP, As pollutant emissions should not be corrected to 0.0 when GNFR is missing
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::TSP), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::TSP), EmissionValue(222.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::As), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::As), EmissionValue(222.0)));

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::gnfr::Aviation), pollutants::NOx), EmissionValue(70.0)));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::DE, EmissionSector(sectors::gnfr::Offroad), pollutants::NOx), EmissionValue(80.0)));

        const auto inv = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary);

        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::NOx), (70.0 / (111.0 + 222.0)) * 111.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::NOx), (70.0 / (111.0 + 222.0)) * 222.0, 0.0);

        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::NOx), (80.0 / (50.0 + 60.0)) * 50.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NOx), (80.0 / (50.0 + 60.0)) * 60.0, 0.0);

        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::Cd), 0.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::Cd), 0.0, 0.0);

        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::TSP), 111.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::TSP), 222.0, 0.0);

        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3ai_i), pollutants::As), 111.0, 0.0);
        checkEmission(inv, EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A3aii_i), pollutants::As), 222.0, 0.0);
    }

    SUBCASE("Use scenario point sources if present")
    {
        TempDir temp("pointscenario");

        // Modify the data root, so we can change the available point source files
        cfg.set_data_root(temp.path());

        const auto pointSourcesPath = temp.path() / "01_data_emissions" / "inventory" / "reporting_2021" / "pointsources" / "BEF";

        SUBCASE("Both scenario and non scenario available")
        {
            const auto pm10ScenarioPath1     = pointSourcesPath / fmt::format("emap_test_PM10_{}_something.csv", static_cast<int>(cfg.year()));
            const auto pm10ScenarioPath2     = pointSourcesPath / fmt::format("emap_test_PM10_{}_something_else.csv", static_cast<int>(cfg.year()));
            const auto pm10OtherScenarioPath = pointSourcesPath / fmt::format("emap_test2_PM10_{}_something.csv", static_cast<int>(cfg.year()));
            const auto pm10NonScenarioPath   = pointSourcesPath / fmt::format("emap_PM10_{}_something.csv", static_cast<int>(cfg.year()));
            const auto noxNonScenarioPath    = pointSourcesPath / fmt::format("emap_NOx_{}_something.csv", static_cast<int>(cfg.year()));

            create_empty_point_source_file(pm10ScenarioPath1);
            create_empty_point_source_file(pm10ScenarioPath2);
            create_empty_point_source_file(pm10OtherScenarioPath);
            create_empty_point_source_file(pm10NonScenarioPath);
            create_empty_point_source_file(noxNonScenarioPath);

            RunSummary summary;
            read_country_point_sources(cfg, countries::BEF, summary);

            CHECK(summary.used_point_sources().size() == 3);
            CHECK(summary.used_point_sources().count(pm10ScenarioPath1) == 1);  // The PM10 scenario specific file should be used, the other PM10 files are ignored
            CHECK(summary.used_point_sources().count(pm10ScenarioPath2) == 1);  // The PM10 scenario specific file should be used, the other PM10 files are ignored
            CHECK(summary.used_point_sources().count(noxNonScenarioPath) == 1); // The regular NOx file will be used as there is no scenario specific one
        }
    }

    SUBCASE("Auto scale point sources, below threshold")
    {
        // empty NFR emissions
        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        ScalingFactors scalings;

        // Two point emissions of which the sum is larger then the total reported emission (threshold = 90% -> 150 / 170 = 88%
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(120.0), Coordinate(10, 10)));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(50.0), Coordinate(10, 20)));

        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(150.0)));

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::gnfr::Shipping), pollutants::PM10), EmissionValue(150.0)));

        CHECK_THROWS_AS(create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary), RuntimeError);
    }

    SUBCASE("Auto scale point sources, above threshold")
    {
        EmissionIdentifier emissionId(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10);
        EmissionIdentifier emissionIdWithUserScaling(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bii), pollutants::PM10);

        // empty NFR emissions
        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        ScalingFactors scalings;
        scalings.add_scaling_factor(ScalingFactor(emissionIdWithUserScaling.country, emissionIdWithUserScaling.sector.nfr_sector(), emissionIdWithUserScaling.sector.gnfr_sector(), emissionIdWithUserScaling.pollutant, EmissionSourceType::Point, 2019_y, 2.0));

        // Two point emissions of which the sum is larger then the total reported emission (threshold = 90% -> 150 / 160 = 93.75%
        pointEmissions.add_emission(EmissionEntry(emissionId, EmissionValue(110.0), Coordinate(10, 10)));
        pointEmissions.add_emission(EmissionEntry(emissionId, EmissionValue(50.0), Coordinate(10, 20)));
        totalEmissions.add_emission(EmissionEntry(emissionId, EmissionValue(150.0)));

        // This point emission has a user scaling of 2.0, this should not influence the auto scaling
        pointEmissions.add_emission(EmissionEntry(emissionIdWithUserScaling, EmissionValue(50.0), Coordinate(10, 20)));
        totalEmissions.add_emission(EmissionEntry(emissionIdWithUserScaling, EmissionValue(70.0)));

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::gnfr::RoadTransport), pollutants::PM10), EmissionValue(150.0)));

        const auto inv = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary);

        checkEmission(inv, emissionId, 0, 150.0);
        checkEmission(inv, emissionIdWithUserScaling, 20, 100.0);

        {
            const auto emission = inv.emissions_with_id(emissionId).front();
            CHECK(emission.point_auto_scaling_factor() == 150.0 / 160.0);
            CHECK(emission.point_user_scaling_factor() == 1.0);
        }

        {
            const auto emission = inv.emissions_with_id(emissionIdWithUserScaling).front();
            CHECK(emission.point_auto_scaling_factor() == 1.0);
            CHECK(emission.point_user_scaling_factor() == 2.0);
        }
    }

    SUBCASE("PMCoarse calculation")
    {
        // empty NFR emissions
        SingleEmissions totalEmissions(date::year(2019)), pointEmissions(date::year(2019));
        ScalingFactors scalings;

        // Two point emissions of which the sum is larger then the total reported emission (threshold = 90% -> 150 / 170 = 88%
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(120.0), Coordinate(10, 10)).with_source_id("id1"));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(50.0), Coordinate(10, 20)).with_source_id("id2"));

        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM2_5), EmissionValue(100.0), Coordinate(10, 10)).with_source_id("id1"));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM2_5), EmissionValue(40.0), Coordinate(10, 20)).with_source_id("id2"));

        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(300.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM2_5), EmissionValue(150.0)));

        SingleEmissions gnfrTotals(date::year(2019));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::gnfr::Shipping), pollutants::PM10), EmissionValue(300.0)));
        gnfrTotals.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::gnfr::Shipping), pollutants::PM2_5), EmissionValue(100.0)));

        const auto inv = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, scalings, cfg, summary);
        checkEmission(inv, EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), 300.0 - 170.0 /* 130 */, 170.0);
        checkEmission(inv, EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM2_5), 150.0 - 140.0 /* 10 */, 140.0);
        checkEmission(inv, EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), 120.0, 30.0);
    }
}

}
