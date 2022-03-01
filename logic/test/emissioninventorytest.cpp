#include "emissioninventory.h"

#include "emap/scalingfactors.h"

#include "runsummary.h"
#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Emission inventory")
{
    RunSummary summary;

    SUBCASE("Subtract point sources in Belgium")
    {
        SingleEmissions totalEmissions, pointEmissions;
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::FR, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::ES, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(222.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx), EmissionValue(100.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PM10), EmissionValue(200.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::PMcoarse), EmissionValue(500.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr2C7d), pollutants::CO), EmissionValue(300.0)));

        SingleEmissions gnfrTotals;
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

        const auto inventory = create_emission_inventory(totalEmissions, gnfrTotals, pointEmissions, diffuseScalings, pointScalings, summary);

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
