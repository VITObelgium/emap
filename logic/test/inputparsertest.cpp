#include "emap/configurationparser.h"
#include "emap/emissions.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "gdx/rasteriterator.h"

#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv)
{
    return RunConfiguration("./data", GridDefinition::Invalid, RunType::Emep, ValidationType::NoValidation, 2016_y, 2021_y, "", sectorInv, pollutantInv, countryInv, "./out");
}

TEST_CASE("Input parsers")
{
    const auto sectorInventory    = parse_sectors(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx", fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "code_conversions.xlsx");
    const auto pollutantInventory = parse_pollutants(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx", fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "code_conversions.xlsx");
    const auto countryInventory   = parse_countries(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx");

    const auto cfg = create_config(sectorInventory, pollutantInventory, countryInventory);

    SUBCASE("Load emissions")
    {
        SUBCASE("nfr sectors")
        {
            auto emissions = parse_emissions(EmissionSector::Type::Nfr, fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt", cfg);
            REQUIRE(emissions.size() == 6);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
            }

            const auto& firstEmission = *emissions.begin();
            //DE;1990;1A1a;PCB;Gg;0.0003408784
            CHECK(firstEmission.country() == countries::DE);
            CHECK(firstEmission.sector().name() == "1A1a");
            CHECK(firstEmission.pollutant() == pollutants::PCBs);
            CHECK(firstEmission.value().amount() == Approx(0.0003408784));
            CHECK(firstEmission.value().unit() == "Gg");
        }

        SUBCASE("gnfr sectors")
        {
            auto emissions = parse_emissions(EmissionSector::Type::Gnfr, fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "gnfr_allyears_2021.txt", cfg);
            REQUIRE(emissions.size() == 4);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Gnfr);
            }

            const auto& firstEmission = *emissions.begin();
            //TJ;1990;A_PublicPower;CO;Gg;1.82364
            CHECK(firstEmission.country() == countries::LI);
            CHECK(firstEmission.sector().name() == "A_PublicPower");
            CHECK(firstEmission.pollutant() == pollutants::CO);
            CHECK(firstEmission.value().amount().value() == Approx(0.001773375));
            CHECK(firstEmission.value().unit() == "Gg");
        }

        SUBCASE("Belgian emissions xlsx (Brussels)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEB_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 517);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEB);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount() == Approx(1.23536111037259));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NMVOC)).value().amount() == Approx(0.123870013146171));
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3di_ii), pollutants::NH3)).empty());
        }

        SUBCASE("Belgian emissions xlsx (Flanders)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 985);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEF);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::SOx)).value().amount() == Approx(0.476353773));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::PCDD_PCDF)).value().amount() == Approx(0.009341 * 1e-15));
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::PCDD_PCDF)).empty());

            // requires unit conversion
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::Cd)).value().amount() == Approx(0.000079609363491485));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::Cd)).value().amount() == Approx(0.000000766324867244634));
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::Cd)).empty());

            // fuel used values override the non fuel used values
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount() == Approx(16.927178482804));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bii), pollutants::NOx)).value().amount() == Approx(10.2568788509957));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biii), pollutants::NOx)).value().amount() == Approx(8.73746975542122));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::NOx)).value().amount() == Approx(0.14127185341995));
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::NOx)).empty());
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bvi), pollutants::NOx)).empty());
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bvii), pollutants::NOx)).empty());

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PM10)).value().amount() == Approx(0.335221546632635));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PM2_5)).value().amount() == Approx(0.214834979288184));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PMcoarse)).value().amount() == Approx(0.120386567344451));

            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM10)).empty());
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM2_5)).empty());
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PMcoarse)).empty());
        }

        SUBCASE("Belgian emissions xlsx (Wallonia)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEW_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 1052);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEW);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr3Dc), pollutants::TSP)).value().amount() == Approx(0.800281464075));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEW, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::Hg)).value().amount() == Approx(0.000000167979106942408)); // only present with fuel used
            CHECK(emissions.emissions_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5C1bii), pollutants::NMVOC)).empty());                                         // empty cell
        }
    }

    SUBCASE("Load point source emissions")
    {
        SUBCASE("nfr sectors")
        {
            const auto emissions = parse_point_sources(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "pointsources" / "pointsource_emissions_2021.csv", cfg);
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
    }

    SUBCASE("Load scaling factors")
    {
        const auto sectorInventory    = parse_sectors(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx", fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "code_conversions.xlsx");
        const auto pollutantInventory = parse_pollutants(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx", fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "code_conversions.xlsx");
        const auto countryInventory   = parse_countries(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx");

        const auto scalings = parse_scaling_factors(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "pointsources" / "scaling_diffuse.csv", cfg);
        REQUIRE(scalings.size() == 4);

        auto iter = scalings.begin();
        CHECK(iter->country() == countries::AL);
        CHECK(iter->sector().name() == "1A2a");
        CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
        CHECK(iter->pollutant() == pollutants::NOx);
        CHECK(iter->factor() == 0.5);
        ++iter;

        CHECK(iter->country() == countries::AL);
        CHECK(iter->sector().name() == "1A2a");
        CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
        CHECK(iter->pollutant() == pollutants::PM10);
        CHECK(iter->factor() == 1.3);
        ++iter;

        CHECK(iter->country() == countries::AM);
        CHECK(iter->sector().name() == "1A1a");
        CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
        CHECK(iter->pollutant() == pollutants::NMVOC);
        CHECK(iter->factor() == 0.8);
        ++iter;

        CHECK(iter->country() == countries::AM);
        CHECK(iter->sector().name() == "1A1a");
        CHECK(iter->sector().type() == EmissionSector::Type::Nfr);
        CHECK(iter->pollutant() == pollutants::NOx);
        CHECK(iter->factor() == 1.5);
    }

    SUBCASE("Load spatial pattern")
    {
        SUBCASE("Flanders")
        {
            const auto spatialPatterns = parse_spatial_pattern_flanders(fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_CO.xlsx", cfg);
            CHECK(spatialPatterns.size() == 25);

            {
                const auto* sp = find_in_container(spatialPatterns, [](const auto& val) {
                    return val.id.sector == EmissionSector(sectors::nfr::Nfr1A5b);
                });

                REQUIRE(sp);

                constexpr const double expectedCellValue = 0.04034311699095;

                const auto cell = sp->raster.metadata().convert_point_to_cell(Point(226000.0, 193000.0));
                CHECK(sp->raster[cell] == Approx(expectedCellValue));
                const auto nodataCell = sp->raster.metadata().convert_point_to_cell(Point(28000.0, 193000.0));
                CHECK(std::isnan(sp->raster[nodataCell]));
                CHECK(138 == std::distance(gdx::value_begin(sp->raster), gdx::value_end(sp->raster)));                                                      // 138 entries should contain data
                CHECK(std::all_of(gdx::value_begin(sp->raster), gdx::value_end(sp->raster), [=](double val) { return val == Approx(expectedCellValue); })); // all the entries have the same value
            }

            {
                const auto* sp = find_in_container(spatialPatterns, [](const auto& val) {
                    return val.id.sector == EmissionSector(sectors::nfr::Nfr2C7d);
                });

                REQUIRE(sp);

                constexpr const double expectedCellValue = 135.0;

                const auto cell = sp->raster.metadata().convert_point_to_cell(Point(204000.0, 196000.0));
                CHECK(sp->raster[cell] == Approx(expectedCellValue));
                CHECK(1 == std::distance(gdx::value_begin(sp->raster), gdx::value_end(sp->raster))); // 1 entry should contain data
            }
        }
    }
}
}