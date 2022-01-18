#include "emap/configurationparser.h"
#include "emap/runconfiguration.h"
#include "infra/exception.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

TEST_CASE("Parse run configuration")
{
    const auto scaleFactors = fs::u8path(TEST_DATA_DIR) / "_input" / "02_scaling" / "historic" / "1990" / "scaling_diffuse.csv";

    const auto expectedDataRoot = fs::u8path(TEST_DATA_DIR) / "_input";
    const auto expectedOutput   = fs::absolute("/temp");

    SUBCASE("valid file")
    {
        std::string_view tomlConfig = R"toml(
            [model]
                grid = "beleuros"
                datapath = "_input"
                type = "emep"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                scalefactors = "{}"
                spatial_patterns = "/emap/emissies/spatpat"
                emissions_output = "/temp"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(fmt::format(tomlConfig, scaleFactors.generic_u8string()), fs::u8path(TEST_DATA_DIR));

        CHECK(config->grid_definition() == GridDefinition::Beleuros);
        CHECK(config->data_root() == expectedDataRoot);
        CHECK(config->run_type() == RunType::Emep);
        CHECK(config->year() == 2020_y);
        CHECK(config->reporting_year() == 2018_y);
        CHECK(config->scenario() == "scenarionaam");
        CHECK(config->spatial_pattern_path() == expectedDataRoot / "03_spatial_disaggregation");

        CHECK(config->output_path() == expectedOutput);
        CHECK(config->validation_type() == ValidationType::SumValidation);
    }

    SUBCASE("invalid file: empty")
    {
        CHECK(!parse_run_configuration("", fs::u8path(TEST_DATA_DIR)).has_value());
    }

    SUBCASE("invalid file: no path quotes")
    {
        std::string_view tomlConfig = R"toml(
            [model]
                grid = "beleuros"
                datapath = _input
                type = "gains"
                year = 2020
                scenario = "scenarionaam"
                scalefactors = "{}"
                spatial_patterns = "./spatpat"
                emissions_output = "/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_AS(parse_run_configuration(fmt::format(tomlConfig, scaleFactors.generic_u8string()), fs::u8path(TEST_DATA_DIR)), RuntimeError);
    }

    SUBCASE("invalid file: year as string")
    {
        std::string_view tomlConfig = R"toml(
            [model]
                grid = "beleuros"
                datapath = "_input"
                type = "gains"
                year = "2020"
                scenario = "scenarionaam"
                scalefactors = "{}"
                spatial_patterns = "./spatpat"
                emissions_output = "/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, scaleFactors.generic_u8string()), fs::u8path(TEST_DATA_DIR)), "Invalid year present in 'input' section, year values should not be quoted (e.g. year = 2020)", RuntimeError);
    }

    SUBCASE("invalid file: scenario is integer")
    {
        std::string_view tomlConfig = R"toml(
            [model]
                grid = "beleuros"
                datapath = "_input"
                type = "gains"
                year = 2020
                report_year = 2020
                scenario = 2010
                scalefactors = "{}"
                spatial_patterns = "./spatpat"
                emissions_output = "/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, scaleFactors.generic_u8string()), fs::u8path(TEST_DATA_DIR)), "'scenario' key value in 'model' section should be a quoted string (e.g. scenario = \"value\")", RuntimeError);
    }
}

}
