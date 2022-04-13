#include "emap/emissioninventory.h"

#include "emap/configurationparser.h"
#include "emap/constants.h"
#include "emap/modelrun.h"
#include "emap/scalingfactors.h"

#include "gdx/denserasterio.h"

#include "brnanalyzer.h"
#include "infra/test/tempdir.h"
#include "outputreaders.h"
#include "runsummary.h"
#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;
using namespace date::literals;

TEST_CASE("Emission inventory [integration]" * skip(true))
{
    SUBCASE("Check emission inventory numbers against manually verified numbers")
    {
        std::string_view configToml = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "./_input"
                type = "emep" # of gains
                year = 2019
                report_year = 2021
                scenario = "test"

            [output]
                path = "./_output_gnfr"
                sector_level = "GNFR"
                create_country_rasters = false
                create_grid_rasters = true

            [options]
                validation = false
        )toml";

        RunSummary summary;
        auto cfg = parse_run_configuration(configToml, fs::u8path(INTEGRATION_TEST_DATA_DIR));

        auto nfrTotalEmissions = read_nfr_emissions(cfg.year(), cfg, summary);
        auto pointSources      = read_country_point_sources(cfg, countries::BEF, summary);

        const auto emissionInv = make_emission_inventory(cfg, summary);

        double pubPowerSum        = 0.0;
        double nfrTotalSum        = 0.0;
        double pointSourcesSum    = 0.0;
        double pointSourcesSumInv = 0.0;

        for (auto& em : emissionInv) {
            if (em.id().sector.gnfr_sector() == sectors::gnfr::PublicPower &&
                em.id().country == countries::BEF &&
                em.id().pollutant == pollutants::SOx) {
                pubPowerSum += em.scaled_diffuse_emissions_sum();
                pointSourcesSumInv += em.scaled_point_emissions_sum();
            }
        }

        auto pointSourceCount = 0;
        for (auto& ps : pointSources) {
            if (ps.id().sector.gnfr_sector() == sectors::gnfr::PublicPower &&
                ps.id().country == countries::BEF &&
                ps.id().pollutant == pollutants::SOx) {
                pointSourcesSum += ps.value().amount().value_or(0.0);
                ++pointSourceCount;
            }
        }

        for (auto& em : nfrTotalEmissions) {
            if (em.id().sector.gnfr_sector() == sectors::gnfr::PublicPower &&
                em.id().country == countries::BEF &&
                em.id().pollutant == pollutants::SOx) {
                nfrTotalSum += em.value().amount().value_or(0.0);
            }
        }

        CHECK(nfrTotalSum == 0.476353773);
        CHECK(pointSourcesSum == Approx(449.078773 / 1000.0));

        CHECK(pubPowerSum == Approx(nfrTotalSum - pointSourcesSum));
        CHECK(pointSourceCount == 39);
        CHECK(pointSourcesSumInv == Approx(pointSourcesSum));
    }

    SUBCASE("Model run")
    {
        TempDir temp("emap_model_run");

        std::string_view configToml = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "./_input"
                type = "emep" # of gains
                year = 2019
                report_year = 2021
                scenario = "test"
                included_pollutants = [ "SOx", "HCB" ]

            [output]
                path = "{}"
                sector_level = "GNFR"
                create_country_rasters = true
                create_grid_rasters = true

            [options]
                validation = false
        )toml";

        const auto outputPath = temp.path() / "output";

        RunSummary summary;
        auto cfg = parse_run_configuration(fmt::format(configToml, outputPath.generic_u8string()), fs::u8path(INTEGRATION_TEST_DATA_DIR));

        CHECK(run_model(cfg, [](const auto&) { return inf::ProgressStatusResult::Continue; }) == EXIT_SUCCESS);

        {
            // SOx checks
            REQUIRE(fs::is_regular_file(outputPath / "SOx_OPS_2019.brn"));
            REQUIRE(fs::is_regular_file(outputPath / "rasters" / "SOx_A_PublicPower_BEF_Vlops 1km.tif"));

            const auto brnEntries = read_brn_output(outputPath / "SOx_OPS_2019.brn");
            BrnAnalyzer analyzer(brnEntries);

            auto raster = gdx::read_dense_raster<double>(outputPath / "rasters" / "SOx_A_PublicPower_BEF_Vlops 1km.tif");

            constexpr auto totalEmissions    = 0.476353773;
            constexpr auto pointEmissionsSum = 449.078773 / 1000.0;                // 0.449078773
            constexpr auto diffuseEmissions  = totalEmissions - pointEmissionsSum; // 0.027275

            CHECK(analyzer.diffuse_emissions_sum(1, 601) == Approx((diffuseEmissions)));
            CHECK(analyzer.point_emissions_sum(1, 601) == Approx(pointEmissionsSum));
            CHECK(raster.sum() == Approx(totalEmissions));
        }

        {
            // HCB checks, lots of zero emissions, only sector 2C1 contains some emissions
            REQUIRE(fs::is_regular_file(outputPath / "HCB_OPS_2019.brn"));
            REQUIRE(fs::is_regular_file(outputPath / "rasters" / "HCB_B_Industry_BEF_Vlops 1km.tif"));

            const auto brnEntries = read_brn_output(outputPath / "HCB_OPS_2019.brn");
            BrnAnalyzer analyzer(brnEntries);

            auto raster = gdx::read_dense_raster<double>(outputPath / "rasters" / "HCB_B_Industry_BEF_Vlops 1km.tif");

            constexpr auto totalEmissions    = 0.0000001719226725;
            constexpr auto pointEmissionsSum = 0.0;
            constexpr auto diffuseEmissions  = totalEmissions - pointEmissionsSum;

            CHECK(analyzer.diffuse_emissions_sum(1, 602) == Approx((diffuseEmissions)));
            CHECK(analyzer.point_emissions_sum(1, 602) == Approx(pointEmissionsSum));
            CHECK(raster.sum() == Approx(totalEmissions));
        }
    }

    SUBCASE("Subtract point sources in Belgium")
    {
        std::string_view configToml = R"toml(
             [model]
                 grid = "vlops1km"
                 datapath = "./_input"
                 type = "emep" # of gains
                 year = 2020
                 report_year = 2022
                 scenario = "test"

             [output]
                 path = "./_output_gnfr"
                 sector_level = "GNFR"
                 create_country_rasters = false
                 create_grid_rasters = true

             [options]
                 validation = false
         )toml";

        RunSummary summary;

        auto cfg            = parse_run_configuration(configToml, fs::u8path(INTEGRATION_TEST_DATA_DIR));
        auto pointEmissions = read_country_point_sources(cfg, countries::BEF, summary);
        double pm10Sum      = 0.0;
        double pm25Sum      = 0.0;
        double pmCoarseSum  = 0.0;
        for (auto& em : pointEmissions) {
            if (em.id().country == countries::BEF && em.id().sector == EmissionSector(sectors::nfr::Nfr1A1a)) {
                if (em.id().pollutant == pollutants::PM10) {
                    pm10Sum += em.value().amount().value_or(0.0);
                } else if (em.id().pollutant == pollutants::PM2_5) {
                    pm25Sum += em.value().amount().value_or(0.0);
                } else if (em.id().pollutant == pollutants::PMcoarse) {
                    pmCoarseSum += em.value().amount().value_or(0.0);
                }
            }
        }

        CHECK(pm10Sum == Approx(32.08043506 / 1000.0));
        CHECK(pm25Sum == Approx(22.70281289 / 1000.0));
        CHECK(pmCoarseSum == Approx(pm10Sum - pm25Sum));
    }
}

}