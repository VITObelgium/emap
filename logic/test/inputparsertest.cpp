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
            CHECK(em.sector().type() == EmissionSector::Type::Nfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2020;AL;1A1a;CO;0;Gg
        CHECK(firstEmission.country().id() == Country::Id::AL);
        CHECK(firstEmission.sector().name() == "1A1a");
        CHECK(firstEmission.pollutant() == Pollutant::CO);
        CHECK(firstEmission.value().amount() == 0.0);
        CHECK(firstEmission.value().unit() == "Gg");
    }

    SUBCASE("gnfr sectors")
    {
        auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "total_emissions_gnfr_2021.csv");
        REQUIRE(emissions.size() == 6);

        for (auto& em : emissions) {
            CHECK(em.sector().type() == EmissionSector::Type::Gnfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2019;AL;A_PublicPower;CO;0.053;Gg
        CHECK(firstEmission.country().id() == Country::Id::AL);
        CHECK(firstEmission.sector().name() == "A_PublicPower");
        CHECK(firstEmission.pollutant() == Pollutant::CO);
        CHECK(firstEmission.value().amount() == 0.053);
        CHECK(firstEmission.value().unit() == "Gg");
    }

    SUBCASE("flanders emissions")
    {
        auto emissions = parse_emissions_flanders(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_CO.xlsx");
        REQUIRE(emissions.size() == 2);

        for (auto& em : emissions) {
            CHECK(em.sector().type() == EmissionSector::Type::Nfr);
            CHECK(em.country().id() == Country::Id::BEF);
        }

        const auto& firstEmission = *emissions.begin();
        CHECK(firstEmission.sector().name() == "1A3di(ii)");
        CHECK(firstEmission.pollutant() == Pollutant::CO);
        CHECK(firstEmission.value().amount() == Approx(0.000002621703567834));
        CHECK(firstEmission.value().unit() == "Gg");
        CHECK(firstEmission.coordinate() == Coordinate(2000, 249000));
    }
}

TEST_CASE("Load point source emissions")
{
    SUBCASE("nfr sectors")
    {
        const auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "pointsource_emissions_2021.csv");
        REQUIRE(emissions.size() == 4);

        int lineNr = 1;
        for (auto& em : emissions) {
            CHECK(em.sector().type() == EmissionSector::Type::Nfr);
            REQUIRE_MESSAGE(em.coordinate().has_value(), fmt::format("Line nr: {} pol {}", lineNr++, em.pollutant()));
        }

        auto iter = emissions.begin();
        CHECK(iter->coordinate() == Coordinate(95820, 173080));
        ++iter;
        CHECK(iter->coordinate() == Coordinate(148450, 197211));
        ++iter;
        CHECK(iter->coordinate() == Coordinate(205000, 209000));
        ++iter;
        CHECK(iter->coordinate() == Coordinate(130643, 159190));
    }

    SUBCASE("flanders point sources")
    {
        const auto emissions = parse_point_sources_flanders(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "2019" / "E-MAP_puntbrongegevens_2019_CO.xlsx");
        REQUIRE(emissions.size() == 2);

        int lineNr = 1;
        for (auto& em : emissions) {
            CHECK(em.pollutant() == Pollutant::CO);
            CHECK(em.sector().type() == EmissionSector::Type::Nfr);
            REQUIRE_MESSAGE(em.coordinate().has_value(), fmt::format("Line nr: {} pol {}", lineNr++, em.pollutant()));
        }

        auto iter = emissions.begin();
        CHECK(iter->sector().name() == "1A2c");
        CHECK(iter->value().amount() == 0.000005);
        CHECK(iter->coordinate() == Coordinate(148404, 197316));
        ++iter;
        CHECK(iter->sector().name() == "1A1a");
        CHECK(iter->value().amount() == 0.00075);
        CHECK(iter->coordinate() == Coordinate(144944, 222999));
    }
}

TEST_CASE("Load scaling factors")
{
    const auto scalings = parse_scaling_factors(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "scaling_diffuse.csv");
    REQUIRE(scalings.size() == 4);

    auto iter = scalings.begin();
    CHECK(iter->country().id() == Country::Id::AL);
    CHECK(iter->sector().name() == "1A2a");
    CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant() == Pollutant::NOx);
    CHECK(iter->factor() == 0.5);
    ++iter;

    CHECK(iter->country().id() == Country::Id::AL);
    CHECK(iter->sector().name() == "1A2a");
    CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant() == Pollutant::PM10);
    CHECK(iter->factor() == 1.3);
    ++iter;

    CHECK(iter->country().id() == Country::Id::AM);
    CHECK(iter->sector().name() == "1A1a");
    CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant() == Pollutant::NMVOC);
    CHECK(iter->factor() == 0.8);
    ++iter;

    CHECK(iter->country().id() == Country::Id::AM);
    CHECK(iter->sector().name() == "1A1a");
    CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
    CHECK(iter->pollutant() == Pollutant::NOx);
    CHECK(iter->factor() == 1.5);
}

}
