#include "emap/emissions.h"
#include "emap/inputparsers.h"

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
            CHECK(em.type == EmissionType::Historic);
            CHECK(em.year == 1990_y);
            CHECK(em.reportingYear == 2020_y);
            CHECK(em.sector.type() == EmissionSector::Type::Nfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2020;AL;1A1a;CO;0;Gg
        CHECK(firstEmission.country == "AL");
        CHECK(firstEmission.sector.name() == "1A1a");
        CHECK(firstEmission.pollutant == "CO");
        CHECK(firstEmission.value.amount() == 0.0);
        CHECK(firstEmission.value.unit() == "Gg");
    }

    SUBCASE("gnfr sectors")
    {
        auto emissions = parse_emissions(fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "total_emissions_gnfr_2021.csv");
        REQUIRE(emissions.size() == 6);

        for (auto& em : emissions) {
            CHECK(em.type == EmissionType::Historic);
            CHECK(em.year == 1990_y);
            CHECK(em.reportingYear == 2019_y);
            CHECK(em.sector.type() == EmissionSector::Type::Gnfr);
        }

        const auto& firstEmission = *emissions.begin();
        //historic;NA;1990;2019;AL;A_PublicPower;CO;0.053;Gg
        CHECK(firstEmission.country == "AL");
        CHECK(firstEmission.sector.name() == "A_PublicPower");
        CHECK(firstEmission.pollutant == "CO");
        CHECK(firstEmission.value.amount() == 0.053);
        CHECK(firstEmission.value.unit() == "Gg");
    }
}

}