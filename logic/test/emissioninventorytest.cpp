#include "emissioninventory.h"

#include "emap/configurationparser.h"
#include "emap/scalingfactors.h"

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

    return RunConfiguration("./data", {}, ModelGrid::Invalid, RunType::Emep, ValidationType::NoValidation, 2016_y, 2021_y, "", {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Emission inventory")
{
    RunSummary summary;

    const auto parametersPath     = fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx");
    const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");
    auto cfg                      = create_config(sectorInventory, pollutantInventory, countryInventory);

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

        ScalingFactors diffuseScalings, pointScalings;
        diffuseScalings.add_scaling_factor(ScalingFactor(EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 0.5));
        pointScalings.add_scaling_factor(ScalingFactor(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), 2.0));

        const auto inventory = create_emission_inventory(totalEmissions, gnfrTotals, {}, pointEmissions, diffuseScalings, pointScalings, cfg, summary);

        auto checkEmission([&inventory](EmissionIdentifier id, double expectedDiffuse, double expectedPoint) {
            const auto emissions = inventory.emissions_with_id(id);
            REQUIRE(emissions.size() == 1);
            CHECK_MESSAGE(emissions.front().scaled_diffuse_emissions_sum() == expectedDiffuse, fmt::format("{}", id));
            CHECK_MESSAGE(emissions.front().scaled_point_emissions_sum() == expectedPoint, fmt::format("{}", id));
            CHECK_MESSAGE(emissions.front().scaled_total_emissions_sum() == expectedDiffuse + expectedPoint, fmt::format("{}", id));
        });

        checkEmission(EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 55.5, 0.0);
        checkEmission(EmissionIdentifier(countries::ES, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 200.0, 0.0); // correction factor should be applied

        checkEmission(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), 88.0, 12.0 /*5 + 7*/);
        checkEmission(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), 99.5, 100.5);
        checkEmission(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), 450, 100.0);
        checkEmission(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr2C7d), pollutants::CO), 300.0, 0.0);
    }
}
}
