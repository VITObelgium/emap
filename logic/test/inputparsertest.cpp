#include "emap/configurationparser.h"
#include "emap/emissions.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "gdx/algo/sum.h"
#include "gdx/rasteriterator.h"
#include "infra/algo.h"

#include "testconfig.h"
#include "testconstants.h"
#include "testprinters.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = "./out";
    outputConfig.outputLevelName = "GNFR";

    return RunConfiguration("./data", {}, ModelGrid::Invalid, RunType::Emep, ValidationType::NoValidation, 2016_y, 2021_y, "", {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Input parsers")
{
    const auto parametersPath     = fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
    const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");

    auto cfg = create_config(sectorInventory, pollutantInventory, countryInventory);

    SUBCASE("Load emissions")
    {
        SUBCASE("nfr sectors")
        {
            // year == 2016, no results
            CHECK(parse_emissions(EmissionSector::Type::Nfr, fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt", cfg.year(), cfg).empty());
            cfg.set_year(1990_y);

            auto emissions = parse_emissions(EmissionSector::Type::Nfr, fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt", cfg.year(), cfg);
            REQUIRE(emissions.size() == 11);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
            }

            const auto em = emissions.emission_with_id(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::PCBs));
            // DE;1990;1A1a;PCB;Gg;0.0003408784
            CHECK(em.country() == countries::DE);
            CHECK(em.sector().name() == "1A1a");
            CHECK(em.pollutant() == pollutants::PCBs);
            CHECK(em.value().amount() == Approx(0.0003408784));
            CHECK(em.value().unit() == "Gg");

            // PMCoarse equals to PM10 when no PM2.5 is available (PMcoarse from the input is not used because it is on the ignore list)
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PM10)).value().amount() == 2.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PMcoarse)).value().amount() == 2.0);

            // PMCoarse equals to PM10 - PM2.5
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PM10)).value().amount() == 5.5);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PM2_5)).value().amount() == 2.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PMcoarse)).value().amount() == 3.5);
        }

        SUBCASE("gnfr sectors")
        {
            cfg.set_year(1990_y);

            auto emissions = parse_emissions(EmissionSector::Type::Gnfr, fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "gnfr_allyears_2021.txt", cfg.year(), cfg);
            REQUIRE(emissions.size() == 4);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Gnfr);
            }

            const auto em = emissions.emission_with_id(EmissionIdentifier(countries::TR, EmissionSector(sectors::gnfr::PublicPower), pollutants::CO));
            // TR;1990;A_PublicPower;CO;Gg;1.82364
            CHECK(em.country() == countries::TR);
            CHECK(em.sector().name() == "A_PublicPower");
            CHECK(em.pollutant() == pollutants::CO);
            CHECK(em.value().amount().value() == Approx(1.82364));
            CHECK(em.value().unit() == "Gg");
        }

        SUBCASE("Belgian emissions xlsx (Brussels)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEB_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3429);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEB);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount() == Approx(1.23536111037259));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NMVOC)).value().amount() == Approx(0.123870013146171));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3di_ii), pollutants::NH3)).value().amount() == 0.0);
        }

        SUBCASE("Belgian emissions xlsx (Flanders)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3429);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEF);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::SOx)).value().amount() == Approx(0.476353773));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::PCDD_PCDF)).value().amount() == Approx(0.009341 * 1e-15));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::PCDD_PCDF)).value().amount() == 0.0);

            // requires unit conversion
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::Cd)).value().amount() == Approx(0.000079609363491485));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::Cd)).value().amount() == Approx(0.000000766324867244634));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::Cd)).value().amount() == 0.0);

            // fuel used values override the non fuel used values
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount() == Approx(16.927178482804));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bii), pollutants::NOx)).value().amount() == Approx(10.2568788509957));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biii), pollutants::NOx)).value().amount() == Approx(8.73746975542122));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3biv), pollutants::NOx)).value().amount() == Approx(0.14127185341995));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bv), pollutants::NOx)).value().amount() == 0.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bvi), pollutants::NOx)).value().amount() == 0.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bvii), pollutants::NOx)).value().amount() == 0.0);

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PM10)).value().amount() == Approx(0.335221546632635));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PM2_5)).value().amount() == Approx(0.214834979288184));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3c), pollutants::PMcoarse)).value().amount() == Approx(0.120386567344451));

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM10)).value().amount() == 0.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM2_5)).value().amount() == 0.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PMcoarse)).value().amount() == 0.0);
        }

        SUBCASE("Belgian emissions xlsx (Wallonia)")
        {
            auto emissions = parse_emissions_belgium(fs::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEW_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3429);

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

            {
                auto noxEmissions = emissions.emissions_with_id(EmissionIdentifier(country::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NOx));
                REQUIRE(noxEmissions.size() == 2);
                CHECK(noxEmissions[0].coordinate().value() == Coordinate(148450, 197211));
                CHECK(noxEmissions[1].coordinate().value() == Coordinate(95820, 173080));
            }
            {
                auto nmvocEmissions = emissions.emissions_with_id(EmissionIdentifier(country::BEF, EmissionSector(sectors::nfr::Nfr1A2c), pollutants::NMVOC));
                REQUIRE(nmvocEmissions.size() == 2);
                CHECK(nmvocEmissions[0].coordinate().value() == Coordinate(130643, 159190));
                CHECK(nmvocEmissions[1].coordinate().value() == Coordinate(205000, 209000));
            }
        }
    }

    SUBCASE("Load scaling factors")
    {
        const auto parametersPath     = fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
        const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
        const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
        const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");

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
        auto rasterForSector = [](const std::vector<SpatialPatternData>& spd, const NfrSector& sector) -> const gdx::DenseRaster<double>& {
            return find_in_container_required(spd, [&sector](const SpatialPatternData& d) {
                       return d.id.sector.nfr_sector() == sector;
                   })
                .raster;
        };

        const auto spatialPatterns = parse_spatial_pattern_flanders(fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_PM10.xlsx", cfg);
        CHECK(spatialPatterns.size() == 52);

        // Flanders
        CHECK(gdx::sum(rasterForSector(spatialPatterns, sectors::nfr::Nfr1A1a)) == Approx(23.5972773909986).epsilon(1e-4));
        // Flanders sector at end of file
        CHECK(gdx::sum(rasterForSector(spatialPatterns, sectors::nfr::Nfr5E)) == Approx(432.989391850553).epsilon(1e-4));
        // Sea sector
        CHECK(rasterForSector(spatialPatterns, sectors::nfr::Nfr1A3dii)(139, 79) == Approx(0.020608247775289));
    }
}
}