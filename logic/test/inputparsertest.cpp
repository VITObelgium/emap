#include "emap/configurationparser.h"
#include "emap/emissions.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "gdx/algo/sum.h"
#include "gdx/rasteriterator.h"
#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/test/printsupport.h"
#include "unitconversion.h"

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

    return RunConfiguration("./data", {}, {}, {}, {}, ModelGrid::Invalid, ValidationType::NoValidation, 2016_y, 2021_y, "", true, 100.0, {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Input parsers")
{
    const auto parametersPath     = file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);

    auto cfg = create_config(sectorInventory, pollutantInventory, countryInventory);

    SUBCASE("Load emissions")
    {
        SUBCASE("nfr sectors")
        {
            // year == 2016, no results
            CHECK(parse_emissions(EmissionSector::Type::Nfr, file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt", cfg.year(), cfg, RespectIgnoreList::Yes).empty());
            cfg.set_year(1990_y);

            auto emissions = parse_emissions(EmissionSector::Type::Nfr, file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt", cfg.year(), cfg, RespectIgnoreList::Yes);
            REQUIRE(emissions.size() == 9);

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

            // PMCoarse should not be read
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PM10)).value().amount() == 2.0);
            CHECK_THROWS(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PMcoarse)));

            // PMCoarse equals to PM10 - PM2.5
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PM10)).value().amount() == 5.5);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PM2_5)).value().amount() == 2.0);
            CHECK_THROWS(emissions.emission_with_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::PMcoarse)));
        }

        SUBCASE("gnfr sectors")
        {
            cfg.set_year(1990_y);

            auto emissions = parse_emissions(EmissionSector::Type::Gnfr, file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "gnfr_allyears_2021.txt", cfg.year(), cfg, RespectIgnoreList::Yes);

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
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEB_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3302);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEB);
                CHECK(em.pollutant() != pollutants::PMcoarse);
            }

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount() == Approx(1.23536111037259));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A4bi), pollutants::NMVOC)).value().amount() == Approx(0.123870013146171));
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEB, EmissionSector(sectors::nfr::Nfr1A3di_ii), pollutants::NH3)).value().amount() == 0.0);
        }

        SUBCASE("Belgian emissions xlsx (Flanders)")
        {
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3302);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEF);
                CHECK(em.pollutant() != pollutants::PMcoarse);
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

            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM10)).value().amount() == 0.0);
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5D3), pollutants::PM2_5)).value().amount() == 0.0);
        }

        SUBCASE("Belgian emissions xlsx (Flanders) no fuel used")
        {
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2022.xlsx", date::year(2022), cfg);
            REQUIRE(emissions.size() == 3302);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEF);
                CHECK(em.pollutant() != pollutants::PMcoarse);
            }

            // Fuel used is empty, make sure it is not used as 0
            CHECK(emissions.emission_with_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr1A3bi), pollutants::NOx)).value().amount().value() == Approx(11.9654509295403));
        }

        SUBCASE("Belgian emissions xlsx (Wallonia)")
        {
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEW_2021.xlsx", date::year(2019), cfg);
            REQUIRE(emissions.size() == 3302);

            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                CHECK(em.country() == countries::BEW);
                CHECK(em.pollutant() != pollutants::PMcoarse);
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
            cfg.set_combine_identical_point_sources(false);

            const auto emissions = parse_point_sources(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "pointsources" / "pointsource_emissions_2021.csv", cfg);
            REQUIRE(emissions.size() == 4);

            int lineNr = 1;
            for (auto& em : emissions) {
                CHECK(em.sector().type() == EmissionSector::Type::Nfr);
                REQUIRE_MESSAGE(em.coordinate().has_value(), fmt::format("Line nr: {} pol {}", lineNr++, em.pollutant()));
            }

            {
                EmissionIdentifier emId(country::BEF, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::NOx);
                REQUIRE(emissions.emissions_with_id(emId).size() == 2);
                REQUIRE(emissions.emissions_with_id_at_coordinate(emId, Coordinate(148450, 197211)).size() == 1);
                REQUIRE(emissions.emissions_with_id_at_coordinate(emId, Coordinate(95820, 173080)).size() == 1);
            }
            {
                EmissionIdentifier emId(country::BEF, EmissionSector(sectors::nfr::Nfr1A2c), pollutants::NMVOC);
                REQUIRE(emissions.emissions_with_id(emId).size() == 2);
                REQUIRE(emissions.emissions_with_id_at_coordinate(emId, Coordinate(130643, 159190)).size() == 1);
                REQUIRE(emissions.emissions_with_id_at_coordinate(emId, Coordinate(205000, 209000)).size() == 1);
            }
        }

        SUBCASE("combine point sources")
        {
            const EmissionIdentifier emissionId(countries::BEF, EmissionSector(sectors::nfr::Nfr3B1b), pollutants::NOx);

            {
                cfg.set_combine_identical_point_sources(true);
                const auto emissions = parse_point_sources(file::u8path(TEST_DATA_DIR) / "point_sources.csv", cfg);
                REQUIRE(emissions.size() == 2);

                CHECK(emissions.emission_with_id_at_coordinate(emissionId, Coordinate(116332.9766, 166636.8709)).value().amount() == Approx(to_giga_gram(1.849281975, "kg/yr")));
                CHECK(emissions.emission_with_id_at_coordinate(emissionId, Coordinate(95419.65, 196533.8)).value().amount() == Approx(to_giga_gram(121.820027319, "kg/yr")));
            }

            {
                cfg.set_combine_identical_point_sources(false);
                const auto emissions = parse_point_sources(file::u8path(TEST_DATA_DIR) / "point_sources.csv", cfg);
                CHECK(emissions.size() == 11);

                CHECK(emissions.emissions_with_id_at_coordinate(emissionId, Coordinate(116332.9766, 166636.8709)).size() == 2);
                CHECK(emissions.emissions_with_id_at_coordinate(emissionId, Coordinate(95419.65, 196533.8)).size() == 9);
            }
        }

        SUBCASE("point sources empty coordinate")
        {
            cfg.set_combine_identical_point_sources(true);
            chrono::DurationRecorder duration;
            CHECK_THROWS_AS(parse_point_sources(file::u8path(TEST_DATA_DIR) / "point_sources_empty_coord.csv", cfg), inf::RuntimeError);
        }

        /*SUBCASE("point sources perf")
        {
            {
                cfg.set_combine_identical_point_sources(true);
                chrono::DurationRecorder duration;
                const auto emissions = parse_point_sources(file::u8path(TEST_DATA_DIR) / "emap_NOx_2021_2024_opslag.csv", cfg);
                Log::info("CSV Point source parsing with combining took: {}", duration.elapsed_time_string());

                REQUIRE(emissions.size() == 28124);
            }

            {
                cfg.set_combine_identical_point_sources(false);
                chrono::DurationRecorder duration;
                const auto emissions = parse_point_sources(file::u8path(TEST_DATA_DIR) / "emap_NOx_2021_2024_opslag.csv", cfg);
                Log::info("CSV Point source parsing without combining  took: {}", duration.elapsed_time_string());

                REQUIRE(emissions.size() == 89358);
            }
        }*/
    }

    SUBCASE("Load scaling factors")
    {
        const auto parametersPath     = file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
        const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");
        const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
        const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);

        const auto scalings = parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "_input" / "02_scaling" / "scaling.xlsx", cfg);
        REQUIRE(scalings.size() == 10);

        {
            // Check that a specific year should overrule the * for years
            EmissionIdentifier id(country::BEF, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::PM10);
            CHECK(scalings.point_scaling_for_id(id, 2015_y) == 2.5);
            CHECK(scalings.point_scaling_for_id(id, 2014_y) == 1.5);
            CHECK(scalings.point_scaling_for_id(id, 2013_y) == 1.5);

            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, 2013_y).has_value());
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id.with_pollutant(pollutants::NOx), 2015_y).has_value());
        }

        {
            // Check that a specific year should overrule year ranges
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::NOx);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, 2009_y).has_value());
            CHECK(scalings.diffuse_scaling_for_id(id, 2010_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2011_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2012_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2013_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2014_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2015_y) == 5);
            CHECK(scalings.diffuse_scaling_for_id(id, 2016_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2017_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2018_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2019_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2020_y) == 3);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, 2021_y).has_value());
        }

        {
            // Check fallback to GNFR
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::NOx);
            CHECK(scalings.diffuse_scaling_for_id(id, 2015_y) == 4);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id.with_sector(EmissionSector(sectors::nfr::Nfr1A1a)), 2021_y).has_value()); // sector from GNFR A should not match the B code
        }

        {
            // Check year range overlap handling, first match will be taken
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr3B1a), pollutants::As);
            CHECK(scalings.diffuse_scaling_for_id(id, 2010_y) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, 2021_y) == 4);
        }

        {
            // Check gnfr/type wildcard
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr3B1a), pollutants::Cd);
            CHECK(scalings.diffuse_scaling_for_id(id, 2005_y) == 0.8);
            CHECK(scalings.point_scaling_for_id(id, 2005_y) == 0.8);
            CHECK(scalings.point_scaling_for_id(id.with_sector(EmissionSector(sectors::nfr::Nfr2D3d)), 2005_y) == 0.5);
        }

        {
            // Check pollutant/country/type wildcard
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5E), pollutants::Cd), 2000_y) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), 2000_y) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), 2000_y) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), 2000_y) == 10);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), 1999_y).has_value());
        }
    }

    SUBCASE("Scalings year is integer")
    {
        CHECK_NOTHROW(parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "scaling_template_year_integer.xlsx", cfg));
    }

    SUBCASE("Gnfr sector mismatch")
    {
        CHECK_THROWS_AS(parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "scaling_template_invalid_gnfr.xlsx", cfg), RuntimeError);
        CHECK_THROWS_AS(parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "scaling_template_gnfr_mismatch.xlsx", cfg), RuntimeError);
        CHECK_THROWS_AS(parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "scaling_template_empty_nfr.xlsx", cfg), RuntimeError);
        CHECK_THROWS_AS(parse_scaling_factors(file::u8path(TEST_DATA_DIR) / "scaling_template_pmcoarse.xlsx", cfg), RuntimeError);
    }

    auto rasterForNfrSector = [](const std::vector<SpatialPatternData>& spd, const NfrSector& sector) -> const gdx::DenseRaster<double>& {
        return find_in_container_required(spd, [&sector](const SpatialPatternData& d) {
                   return d.id.sector.nfr_sector() == sector;
               })
            .raster;
    };

    auto rasterForGnfrSector = [](const std::vector<SpatialPatternData>& spd, const GnfrSector& sector) -> const gdx::DenseRaster<double>& {
        return find_in_container_required(spd, [&sector](const SpatialPatternData& d) {
                   return d.id.sector.gnfr_sector() == sector;
               })
            .raster;
    };

    SUBCASE("Load spatial patterns with gnfr sector")
    {
        const auto spatialPatterns = parse_spatial_pattern_flanders(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NH3.xlsx", cfg);
        CHECK(spatialPatterns.size() == 2);

        // Flanders
        CHECK(gdx::sum(rasterForNfrSector(spatialPatterns, sectors::nfr::Nfr1A1a)) == Approx(7.47416E-05).epsilon(1e-4));
        // Flanders sector at end of file
        CHECK(gdx::sum(rasterForGnfrSector(spatialPatterns, sectors::gnfr::AgriLiveStock)) == Approx(18.0750674).epsilon(1e-4));
    }

    SUBCASE("Load spatial pattern with gnfr sector fallback")
    {
        // Nfr3B1a is not present in the file, but K_AgriLiveStock is
        const auto spatialPattern = parse_spatial_pattern_flanders(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NH3.xlsx", EmissionSector(sectors::nfr::Nfr3B1a), cfg);
        CHECK(gdx::sum(spatialPattern) == Approx(18.0750674).epsilon(1e-4));
    }
}

}
