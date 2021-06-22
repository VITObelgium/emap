#include "emap/emissions.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

TEST_CASE("Load emissions")
{
    SUBCASE("nfr sectors")
    {
        auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "total_emissions_nfr_2021.csv");
        REQUIRE(emissions.size() == 6);

        for (auto& em : emissions) {
            CHECK(em.sector.type() == EmissionSector::Type::Nfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2020;AL;1A1a;CO;0;Gg
        CHECK(firstEmission.country == "AL");
        CHECK(firstEmission.sector.name() == "1A1a");
        CHECK(firstEmission.pollutant == Pollutant::CO);
        CHECK(firstEmission.value.amount() == 0.0);
        CHECK(firstEmission.value.unit() == "Gg");
    }

    SUBCASE("gnfr sectors")
    {
        auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "total_emissions_gnfr_2021.csv");
        REQUIRE(emissions.size() == 6);

        for (auto& em : emissions) {
            CHECK(em.sector.type() == EmissionSector::Type::Gnfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2019;AL;A_PublicPower;CO;0.053;Gg
        CHECK(firstEmission.country == "AL");
        CHECK(firstEmission.sector.name() == "A_PublicPower");
        CHECK(firstEmission.pollutant == Pollutant::CO);
        CHECK(firstEmission.value.amount() == 0.053);
        CHECK(firstEmission.value.unit() == "Gg");
    }
}

TEST_CASE("Load point source emissions")
{
    SUBCASE("nfr sectors")
    {
        auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "pointsource_emissions_2021.csv");
        REQUIRE(emissions.size() == 4);

        int lineNr = 1;
        for (auto& em : emissions) {
            CHECK(em.sector.type() == EmissionSector::Type::Nfr);
            REQUIRE_MESSAGE(em.coordinate.has_value(), fmt::format("Line nr: {} pol {}", lineNr++, em.pollutant));
        }

        auto iter = emissions.begin();
        CHECK(iter->coordinate == Coordinate(95820, 173080));
        ++iter;
        CHECK(iter->coordinate == Coordinate(148450, 197211));
        ++iter;
        CHECK(iter->coordinate == Coordinate(205000, 209000));
        ++iter;
        CHECK(iter->coordinate == Coordinate(130643, 159190));
    }
}

TEST_CASE("Load scaling factors")
{
    auto emissions = parse_scaling_factors(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "scaling_diffuse.csv");
    REQUIRE(emissions.size() == 4);

    auto iter = emissions.begin();
    CHECK(iter->country == Country::AL);
    CHECK(iter->sector.name() == "1A2a");
    CHECK(iter->sector.type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant == Pollutant::NOx);
    CHECK(iter->factor == 1.0);
    ++iter;

    CHECK(iter->country == Country::AL);
    CHECK(iter->sector.name() == "1A2a");
    CHECK(iter->sector.type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant == Pollutant::PM10);
    CHECK(iter->factor == 1.0);
    ++iter;

    CHECK(iter->country == Country::AM);
    CHECK(iter->sector.name() == "1A1a");
    CHECK(iter->sector.type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant == Pollutant::NMVOC);
    CHECK(iter->factor == 0.8);
    ++iter;

    CHECK(iter->country == Country::AM);
    CHECK(iter->sector.name() == "1A1a");
    CHECK(iter->sector.type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant == Pollutant::NOx);
    CHECK(iter->factor == 1.5);
}

}
