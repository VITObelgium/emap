﻿#include "emap/configurationparser.h"
#include "emap/runconfiguration.h"
#include "infra/exception.h"
#include "testconstants.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

TEST_CASE("Parse run configuration")
{
    const auto scaleFactors = file::u8path(TEST_DATA_DIR) / "_input" / "02_scaling" / "historic" / "1990" / "scaling_diffuse.csv";

    const auto expectedDataRoot = file::u8path(TEST_DATA_DIR) / "_input";
    const auto expectedOutput   = fs::absolute("/temp");

    SUBCASE("valid file")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                type = "emep"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                scalefactors = "{}"
                combine_identical_point_sources = true
                spatial_boundaries_filename = "spatial_bounds.geojson"
            
            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR));

        CHECK(config.model_grid() == ModelGrid::Vlops1km);
        CHECK(config.data_root() == expectedDataRoot);
        CHECK(config.year() == 2020_y);
        CHECK(config.reporting_year() == 2018_y);
        CHECK(config.scenario() == "scenarionaam");
        CHECK(config.combine_identical_point_sources() == true);
        CHECK(config.spatial_pattern_path() == expectedDataRoot / "03_spatial_disaggregation");
        CHECK(config.boundaries_vector_path() == expectedDataRoot / "03_spatial_disaggregation" / "boundaries" / "spatial_bounds.geojson");
        CHECK(config.eez_boundaries_vector_path() == expectedDataRoot / "03_spatial_disaggregation" / "boundaries" / "boundaries_incl_EEZ.gpkg");

        CHECK(config.output_path() == expectedOutput);
        CHECK(config.validation_type() == ValidationType::SumValidation);

        CHECK(config.included_pollutants() == container_as_vector(config.pollutants().list()));

        CHECK(config.sectors().nfr_sector_from_string("1A3di(ii)").destination() == EmissionDestination::Eez);
        CHECK(config.sectors().nfr_sector_from_string("1A3di(ii)").has_land_destination());
    }

    SUBCASE("valid file with specified pollutants")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                type = "emep"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                scalefactors = "{}"
                included_pollutants = ["CO", "NOx", "NMVOC"]
                combine_identical_point_sources = false
            
            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR));

        CHECK(config.model_grid() == ModelGrid::Vlops1km);
        CHECK(config.data_root() == expectedDataRoot);
        CHECK(config.year() == 2020_y);
        CHECK(config.reporting_year() == 2018_y);
        CHECK(config.scenario() == "scenarionaam");
        CHECK(config.combine_identical_point_sources() == false);
        CHECK(config.spatial_pattern_path() == expectedDataRoot / "03_spatial_disaggregation");

        CHECK(config.output_path() == expectedOutput);
        CHECK(config.validation_type() == ValidationType::SumValidation);
        CHECK(config.included_pollutants() == std::vector<Pollutant>{pollutants::CO, pollutants::NOx, pollutants::NMVOC});
    }

    SUBCASE("scenario processing")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                type = "emep"
                year = 2022
                report_year = 2021
                scenario = "scen"
                scalefactors = "{}"
                included_pollutants = ["CO", "NOx", "NMVOC"]
            
            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR));

        CHECK(config.scenario() == "scen");
        CHECK(config.combine_identical_point_sources() == true);
        // Scenario specific input available
        CHECK(config.total_emissions_path_nfr_belgium(country::BEB) == expectedDataRoot / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEB_2021_scen.xlsx");
        CHECK(config.total_emissions_path_gnfr(config.reporting_year()) == expectedDataRoot / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "gnfr_allyears_2021_scen.txt");
        // No scenario specific input available
        CHECK(config.total_emissions_path_nfr_belgium(country::BEF) == expectedDataRoot / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "BEF_2021.xlsx");
        CHECK(config.total_emissions_path_nfr(1990_y, config.reporting_year()) == expectedDataRoot / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt");
    }

    SUBCASE("invalid file: empty")
    {
        CHECK_THROWS_AS(parse_run_configuration("", file::u8path(TEST_DATA_DIR)), RuntimeError);
    }

    SUBCASE("invalid file: no path quotes")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = _input
                type = "gains"
                year = 2020
                scenario = "scenarionaam"
                scalefactors = "{}"

            [output]
                path = "/temp"
                sector_level = "GNFR"
        )toml";

        CHECK_THROWS_AS(parse_run_configuration(fmt::format(tomlConfig, file::generic_u8string(scaleFactors)), file::u8path(TEST_DATA_DIR)), RuntimeError);
    }

    SUBCASE("invalid file: year as string")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                type = "gains"
                year = "2020"
                scenario = "scenarionaam"
                scalefactors = "{}"

            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR)), "Invalid year present in 'input' section, year values should not be quoted (e.g. year = 2020)", RuntimeError);
    }

    SUBCASE("invalid file: scenario is integer")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                type = "gains"
                year = 2020
                report_year = 2020
                scenario = 2010
                scalefactors = "{}"

            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR)), "'scenario' key value in 'model' section should be a quoted string (e.g. scenario = \"value\")", RuntimeError);
    }
}
}
