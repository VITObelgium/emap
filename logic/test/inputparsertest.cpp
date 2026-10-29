#include "emap/configurationparser.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"
#include "gdx/algo/sum.h"
#include "gdx/rasteriterator.h"
#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/crs.h"
#include "infra/tempdir.h"
#include "unitconversion.h"
#include "xlsxworkbook.h"

#include "testconfig.h"
#include "testconstants.h"
#include "testprinters.h"

#include <doctest/doctest.h>
#include <initializer_list>
#include <limits>
#include <utility>

namespace emap::test {

using namespace inf;
using namespace doctest;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv, int32_t flandersInputEpsg = crs::epsg::BelgianLambert72)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = "./out";
    outputConfig.outputLevelName = "GNFR";

    return RunConfiguration("./data", {}, {}, {}, {}, ModelGrid::Invalid, ValidationType::NoValidation, chrono::year(2016), chrono::year(2021), "", true, 100.0, {}, sectorInv, pollutantInv, countryInv, outputConfig, std::nullopt, flandersInputEpsg);
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
            cfg.set_year(chrono::year(1990));

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
            cfg.set_year(chrono::year(1990));

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
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEB_2021.xlsx", chrono::year(2019), cfg);
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
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2021.xlsx", chrono::year(2019), cfg);
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
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2022.xlsx", chrono::year(2022), cfg);
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
            auto emissions = parse_emissions_belgium(file::u8path(TEST_DATA_DIR) / "_input" / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEW_2021.xlsx", chrono::year(2019), cfg);
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
            CHECK(scalings.point_scaling_for_id(id, chrono::year(2015)) == 2.5);
            CHECK(scalings.point_scaling_for_id(id, chrono::year(2014)) == 1.5);
            CHECK(scalings.point_scaling_for_id(id, chrono::year(2013)) == 1.5);

            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, chrono::year(2013)).has_value());
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id.with_pollutant(pollutants::NOx), chrono::year(2015)).has_value());
        }

        {
            // Check that a specific year should overrule year ranges
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr1A2b), pollutants::NOx);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, chrono::year(2009)).has_value());
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2010)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2011)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2012)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2013)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2014)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2015)) == 5);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2016)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2017)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2018)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2019)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2020)) == 3);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id, chrono::year(2021)).has_value());
        }

        {
            // Check fallback to GNFR
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::NOx);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2015)) == 4);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(id.with_sector(EmissionSector(sectors::nfr::Nfr1A1a)), chrono::year(2021)).has_value()); // sector from GNFR A should not match the B code
        }

        {
            // Check year range overlap handling, first match will be taken
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr3B1a), pollutants::As);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2010)) == 3);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2021)) == 4);
        }

        {
            // Check gnfr/type wildcard
            EmissionIdentifier id(countries::NL, EmissionSector(sectors::nfr::Nfr3B1a), pollutants::Cd);
            CHECK(scalings.diffuse_scaling_for_id(id, chrono::year(2005)) == 0.8);
            CHECK(scalings.point_scaling_for_id(id, chrono::year(2005)) == 0.8);
            CHECK(scalings.point_scaling_for_id(id.with_sector(EmissionSector(sectors::nfr::Nfr2D3d)), chrono::year(2005)) == 0.5);
        }

        {
            // Check pollutant/country/type wildcard
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5E), pollutants::Cd), chrono::year(2000)) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::NL, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), chrono::year(2000)) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::BEF, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), chrono::year(2000)) == 10);
            CHECK(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), chrono::year(2000)) == 10);
            CHECK_FALSE(scalings.diffuse_scaling_for_id(EmissionIdentifier(countries::DE, EmissionSector(sectors::nfr::Nfr5E), pollutants::CO), chrono::year(1999)).has_value());
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
        const auto spatialPatterns = parse_spatial_pattern_flanders(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NH3.xlsx", cfg, grid_data(GridDefinition::Flanders1km).meta);
        REQUIRE(spatialPatterns.size() == 2);
        for (const auto& pattern : spatialPatterns) {
            CHECK(pattern.raster.metadata() == grid_data(GridDefinition::Flanders1km).meta);
        }

        // Flanders
        CHECK(gdx::sum(rasterForNfrSector(spatialPatterns, sectors::nfr::Nfr1A1a)) == Approx(7.47416E-05).epsilon(1e-4));
        // Flanders sector at end of file
        CHECK(gdx::sum(rasterForGnfrSector(spatialPatterns, sectors::gnfr::AgriLiveStock)) == Approx(18.0750674).epsilon(1e-4));
    }

    SUBCASE("Load spatial pattern with gnfr sector fallback")
    {
        // Nfr3B1a is not present in the file, but K_AgriLiveStock is
        const auto spatialPattern = parse_spatial_pattern_flanders(file::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "bef" / "reporting_2021" / "2019" / "Emissies per km2 excl puntbrongegevens_2019_NH3.xlsx", EmissionSector(sectors::nfr::Nfr3B1a), cfg, grid_data(GridDefinition::Flanders1km).meta);
        CHECK(spatialPattern.metadata() == grid_data(GridDefinition::Flanders1km).meta);
        CHECK(gdx::sum(spatialPattern) == Approx(18.0750674).epsilon(1e-4));
    }
}

TEST_CASE("Flanders spatial patterns reproject directly to the output grid")
{
    const auto dataPath           = file::u8path(TEST_DATA_DIR);
    const auto parametersPath     = dataPath / "_input" / "05_model_parameters";
    const auto countryInventory   = parse_countries(parametersPath / "id_nummers.xlsx");
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    const auto cfg                = create_config(sectorInventory, pollutantInventory, countryInventory, crs::epsg::BelgianLambert2008);
    const EmissionSector nfrSector(sectors::nfr::Nfr1A1a);
    const EmissionSector fallbackSector(sectors::nfr::Nfr3B1a);
    const auto makeGrid = [](int rows, int cols, double x, double y, double cellSize, int epsg) {
        GeoMetadata meta(rows, cols, x, y, cellSize, std::numeric_limits<double>::quiet_NaN());
        meta.set_projection_from_epsg(epsg);
        return meta;
    };
    const auto meta       = makeGrid(5, 5, 649000.0, 699000.0, 1000.0, crs::epsg::BelgianLambert2008);
    const auto firstCell  = meta.convert_point_to_cell(Point<double>(650500.0, 700500.0));
    const auto secondCell = meta.convert_point_to_cell(Point<double>(651500.0, 701500.0));
    const auto gnfrFirst  = meta.convert_point_to_cell(Point<double>(652500.0, 700500.0));
    const auto gnfrSecond = meta.convert_point_to_cell(Point<double>(650500.0, 702500.0));
    TempDir temp("flanders_native");
    const auto csvPath       = temp.path() / "patterns.csv";
    const std::string header = "year,nfr_sector,gnfr_sector,pollutant,x_lambert,y_lambert,emission\n";
    const auto checkRaster   = [](const gdx::DenseRaster<double>& raster, const GeoMetadata& target, std::initializer_list<std::pair<Cell, double>> expected) {
        CHECK(raster.metadata() == target);
        double sum = 0.0;
        for (const auto& [cell, value] : expected) {
            CHECK_FALSE(raster.is_nodata(cell));
            CHECK(raster[cell] == Approx(value));
            sum += value;
        }
        size_t populated = 0;
        for (const auto cell : gdx::RasterCells(raster)) {
            populated += !raster.is_nodata(cell);
        }
        CHECK(populated == expected.size());
        CHECK(gdx::sum(raster) == Approx(sum));
    };
    const auto checkSingle = [&](const fs::path& path, const RunConfiguration& sourceCfg, const GeoMetadata& target, Cell cell, double weight) {
        const auto patterns = parse_spatial_pattern_flanders(path, sourceCfg, target);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{cell, weight}});
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, sourceCfg, target), target, {{cell, weight}});
    };

    SUBCASE("Both overloads retain native locations, weights and GNFR fallback")
    {
        const auto path     = dataPath / "flanders_native_3812.csv";
        const auto patterns = parse_spatial_pattern_flanders(path, cfg, meta);
        REQUIRE(patterns.size() == 2);
        for (const auto& pattern : patterns) {
            CHECK(pattern.id.country == countries::BEF);
            CHECK(pattern.id.pollutant == pollutants::NH3);
            CHECK(pattern.year == chrono::year(2019));
        }
        CHECK(patterns[0].id.sector == nfrSector);
        CHECK(patterns[1].id.sector == EmissionSector(sectors::gnfr::AgriLiveStock));
        checkRaster(patterns[0].raster, meta, {{firstCell, 2.5}, {secondCell, 3.5}});
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, cfg, meta), meta, {{firstCell, 2.5}, {secondCell, 3.5}});
        checkRaster(patterns[1].raster, meta, {{gnfrFirst, 7.0}, {gnfrSecond, 11.0}});
        checkRaster(parse_spatial_pattern_flanders(path, fallbackSector, cfg, meta), meta, {{gnfrFirst, 7.0}, {gnfrSecond, 11.0}});
    }

    SUBCASE("Original parser API uses the Lambert2008 pattern grid for Lambert2008 input")
    {
        const auto& target = grid_data(GridDefinition::Flanders1kmLambert2008).meta;
        CHECK(target == makeGrid(154, 260, 500000.0, 642000.0, 1000.0, crs::epsg::BelgianLambert2008));
        const auto path     = dataPath / "flanders_native_origin_3812.csv";
        const auto patterns = parse_spatial_pattern_flanders(path, cfg);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{Cell(130, 149), 5.0}});
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, cfg), target, {{Cell(130, 149), 5.0}});
    }

    SUBCASE("Original parser API retains the Lambert72 pattern grid for EPSG 31370")
    {
        const auto sourceCfg = create_config(sectorInventory, pollutantInventory, countryInventory, crs::epsg::BelgianLambert72);
        const auto& target   = grid_data(GridDefinition::Flanders1km).meta;
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,149000,165000,5\n");
        const auto patterns = parse_spatial_pattern_flanders(csvPath, sourceCfg);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{Cell(130, 149), 5.0}});
        checkRaster(parse_spatial_pattern_flanders(csvPath, nfrSector, sourceCfg), target, {{Cell(130, 149), 5.0}});
    }

    SUBCASE("Original parser API uses Lambert2008 for other input projections")
    {
        const auto sourceCfg = create_config(sectorInventory, pollutantInventory, countryInventory, crs::epsg::WGS84WebMercator);
        const auto& target   = grid_data(GridDefinition::Flanders1kmLambert2008).meta;
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,485265.686825,6585607.162460,5\n");
        const auto patterns = parse_spatial_pattern_flanders(csvPath, sourceCfg);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{Cell(130, 149), 5.0}});
        checkRaster(parse_spatial_pattern_flanders(csvPath, nfrSector, sourceCfg), target, {{Cell(130, 149), 5.0}});
    }

    SUBCASE("The output half-cell offset follows reprojection to arbitrary output grids")
    {
        // EPSG:3812's origin (649328, 665262) is lon 4.35921583333333, lat 50.797815.
        // The fixture stores that point; the offset uses each target's cell size after reprojection.
        const auto path = dataPath / "flanders_native_origin_3812.csv";
        for (const auto& [target, cell] : {
                 std::pair{makeGrid(4, 4, 649000.0, 665000.0, 250.0, crs::epsg::BelgianLambert2008), Cell(2, 1)},
                 std::pair{makeGrid(3, 3, 148000.0, 164000.0, 1000.0, crs::epsg::BelgianLambert72), Cell(1, 1)},
                 std::pair{makeGrid(4, 4, 4.358, 50.797, 0.0005, crs::epsg::WGS84), Cell(1, 2)},
                 // Spherical Mercator of the independent lon/lat is (485265.686825, 6585607.162460).
                 std::pair{makeGrid(4, 4, 485000.0, 6585000.0, 250.0, crs::epsg::WGS84WebMercator), Cell(1, 1)}}) {
            INFO(target.projection);
            checkSingle(path, cfg, target, cell, 5.0);
        }
    }

    SUBCASE("Matching CRS skips transformation; coarse-cell weights add within the output extent")
    {
        const auto target = makeGrid(2, 2, 800000.0, 900000.0, 2000.0, crs::epsg::BelgianLambert2008);
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,800000.25,900000.75,2.5\n"
                                              "2019,1A1a,,NH3,800000.25,900000.75,3.5\n"
                                              "2019,1A1a,,NH3,801000,901000,4\n"
                                              "2019,1A1a,,NH3,798000,900000,99\n"
                                              "2019,1A1a,,NH3,804000,900000,99\n"
                                              "2019,1A1a,,NH3,800000,898000,99\n"
                                              "2019,1A1a,,NH3,800000,904000,99\n");
        const auto patterns = parse_spatial_pattern_flanders(csvPath, cfg, target);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{Cell(1, 0), 6.0}, {Cell(1, 1), 4.0}});
        checkRaster(parse_spatial_pattern_flanders(csvPath, nfrSector, cfg, target), target, {{Cell(1, 0), 6.0}, {Cell(1, 1), 4.0}});
    }

    SUBCASE("Fractional EPSG:3857 source coordinates need no input grid or alignment")
    {
        const auto mercatorCfg = create_config(sectorInventory, pollutantInventory, countryInventory, crs::epsg::WGS84WebMercator);
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,485265.686824642,6585607.162459933,5\n");
        checkSingle(csvPath, mercatorCfg, makeGrid(4, 4, 485000.0, 6585000.0, 250.0, crs::epsg::WGS84WebMercator), Cell(1, 1), 5.0);
        checkSingle(csvPath, mercatorCfg, makeGrid(4, 4, 4.358, 50.797, 0.0005, crs::epsg::WGS84), Cell(1, 2), 5.0);
    }

    SUBCASE("Rectangular output cells supply independent offsets for each axis")
    {
        GeoMetadata target(2, 2, 100.0, 200.0, GeoMetadata::CellSize{200.0, -100.0}, std::numeric_limits<double>::quiet_NaN());
        target.set_projection_from_epsg(crs::epsg::BelgianLambert2008);
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,100,200,2\n2019,1A1a,,NH3,300,300,3\n");
        const auto patterns = parse_spatial_pattern_flanders(csvPath, cfg, target);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, target, {{Cell(1, 0), 2.0}, {Cell(0, 1), 3.0}});
        checkRaster(parse_spatial_pattern_flanders(csvPath, nfrSector, cfg, target), target, {{Cell(1, 0), 2.0}, {Cell(0, 1), 3.0}});
    }

    SUBCASE("Consumed coordinates must be numeric and finite, but need not be aligned")
    {
        for (const auto& [x, y] : {std::pair{"", "700000"}, std::pair{"650000", ""}, std::pair{"invalid", "700000"},
                                   std::pair{"nan", "700000"}, std::pair{"650000", "inf"}, std::pair{"-inf", "700000"}}) {
            INFO(fmt::format("x='{}', y='{}'", x, y));
            file::write_as_text(csvPath, header + fmt::format("2019,1A1a,,NH3,{},{},2.5\n", x, y));
            CHECK_THROWS_AS(parse_spatial_pattern_flanders(csvPath, cfg, meta), RuntimeError);
            CHECK_THROWS_AS(parse_spatial_pattern_flanders(csvPath, nfrSector, cfg, meta), RuntimeError);
        }
        file::write_as_text(csvPath, header + "2019,1A1a,,NH3,650001.25,700000.75,2.5\n");
        checkSingle(csvPath, cfg, meta, firstCell, 2.5);
    }

    SUBCASE("Excluded or unrelated sectors do not validate unused coordinates")
    {
        checkSingle(dataPath / "flanders_native_excluded_3812.csv", cfg, meta, firstCell, 2.5);
        const auto path = dataPath / "flanders_native_unrelated_3812.csv";
        CHECK_THROWS_AS(parse_spatial_pattern_flanders(path, cfg, meta), RuntimeError);
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, cfg, meta), meta, {{firstCell, 2.5}});
    }

    SUBCASE("Empty input retains the requested output metadata and nodata gaps")
    {
        const auto path = dataPath / "flanders_native_empty.csv";
        CHECK(parse_spatial_pattern_flanders(path, cfg, meta).empty());
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, cfg, meta), meta, {});
    }

    SUBCASE("Numeric XLSX coordinates use the same output cells as CSV")
    {
        const auto path = temp.path() / "patterns.xlsx";
        {
            xl::WorkBook workbook(path);
            auto* worksheet = workbook_add_worksheet(workbook, "Patterns");
            REQUIRE(worksheet != nullptr);
            lxw_col_t column = 0;
            for (const auto* name : {"year", "nfr_sector", "gnfr_sector", "pollutant", "x_lambert", "y_lambert", "emission"}) {
                REQUIRE(worksheet_write_string(worksheet, 0, column++, name, nullptr) == LXW_NO_ERROR);
            }
            for (lxw_row_t row = 1; row <= 2; ++row) {
                REQUIRE(worksheet_write_number(worksheet, row, 0, 2019.0, nullptr) == LXW_NO_ERROR);
                REQUIRE(worksheet_write_string(worksheet, row, 1, "1A1a", nullptr) == LXW_NO_ERROR);
                REQUIRE(worksheet_write_string(worksheet, row, 3, "NH3", nullptr) == LXW_NO_ERROR);
                REQUIRE(worksheet_write_number(worksheet, row, 4, 649000.0 + row * 1000.0, nullptr) == LXW_NO_ERROR);
                REQUIRE(worksheet_write_number(worksheet, row, 5, 699000.0 + row * 1000.0, nullptr) == LXW_NO_ERROR);
                REQUIRE(worksheet_write_number(worksheet, row, 6, 1.5 + row, nullptr) == LXW_NO_ERROR);
            }
        } // Close the workbook before GDAL opens it.
        const auto patterns = parse_spatial_pattern_flanders(path, cfg, meta);
        REQUIRE(patterns.size() == 1);
        checkRaster(patterns.front().raster, meta, {{firstCell, 2.5}, {secondCell, 3.5}});
        checkRaster(parse_spatial_pattern_flanders(path, nfrSector, cfg, meta), meta, {{firstCell, 2.5}, {secondCell, 3.5}});
    }
}
}
