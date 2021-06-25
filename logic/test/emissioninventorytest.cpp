#include "emissioninventory.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Emission inventory")
{
    SUBCASE("Subtract point sources in Belgium")
    {
        SingleEmissions totalEmissions, pointEmissions;
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::FR, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(111.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::ES, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(222.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(100.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEW, EmissionSector(GnfrSector::RoadTransport), Pollutant::PM10), EmissionValue(200.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEB, EmissionSector(GnfrSector::RoadTransport), Pollutant::PMcoarse), EmissionValue(500.0)));
        totalEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::Industry), Pollutant::CO), EmissionValue(300.0)));

        // point emissions outside of belgium should not be used
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::FR, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(11.0), Coordinate(10, 10)));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::ES, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(22.0), Coordinate(11, 11)));

        // 2 point emissions for Road NOx
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(5.0), Coordinate(100, 100)));
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), EmissionValue(7.0), Coordinate(101, 102)));

        // 1 point emissions for Road PM10
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEW, EmissionSector(GnfrSector::RoadTransport), Pollutant::PM10), EmissionValue(100.5), Coordinate(102, 103)));

        // 1 point emissions for Road PMcoarse in Brussels
        pointEmissions.add_emission(EmissionEntry(EmissionIdentifier(Country::Id::BEB, EmissionSector(GnfrSector::RoadTransport), Pollutant::PMcoarse), EmissionValue(50.0), Coordinate(102, 103)));

        // no point emissions for Industry CO

        const auto inventory = create_emission_inventory(totalEmissions, pointEmissions);

        auto checkEmission([&inventory](EmissionIdentifier id, double expectedDiffuse, double expectedPoint) {
            const auto emissions = inventory.emissions_with_id(id);
            REQUIRE(emissions.size() == 1);
            CHECK_MESSAGE(emissions.front().diffuse_emissions() == expectedDiffuse, fmt::format("{}", id));
            CHECK_MESSAGE(emissions.front().point_emissions() == expectedPoint, fmt::format("{}", id));
            CHECK_MESSAGE(emissions.front().total_emissions() == expectedDiffuse + expectedPoint, fmt::format("{}", id));
        });

        checkEmission(EmissionIdentifier(Country::Id::FR, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), 111.0, 0.0);
        checkEmission(EmissionIdentifier(Country::Id::ES, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), 222.0, 0.0);

        checkEmission(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::RoadTransport), Pollutant::NOx), 88.0, 12.0 /*5 + 7*/);
        checkEmission(EmissionIdentifier(Country::Id::BEW, EmissionSector(GnfrSector::RoadTransport), Pollutant::PM10), 99.5, 100.5);
        checkEmission(EmissionIdentifier(Country::Id::BEB, EmissionSector(GnfrSector::RoadTransport), Pollutant::PMcoarse), 450, 50.0);
        checkEmission(EmissionIdentifier(Country::Id::BEF, EmissionSector(GnfrSector::Industry), Pollutant::CO), 300.0, 0.0);
    }
}
}
