#include "emap/runconfigurationparser.h"
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
    const auto scaleFactors = fs::u8path(TEST_DATA_DIR) / "input" / "emission_data" / "historic" / "1990" / "scaling_diffuse.csv";

    SUBCASE("valid file")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = "C:/emap/emissies/"
                type = "emep"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                scalefactors = "{}" 

            [output]
                path = "c:/temp"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string())));

        CHECK(config.grid_definition() == GridDefinition::Beleuros);
        CHECK(config.data_root() == fs::path("C:/emap/emissies/"));
        CHECK(config.run_type() == RunType::Emep);
        CHECK(config.year() == 2020_y);
        CHECK(config.reporting_year().has_value());
        CHECK(config.reporting_year() == 2018_y);
        CHECK(config.scenario() == "scenarionaam");

        CHECK(config.output_path() == fs::path("c:/temp"));
        CHECK(config.validation_type() == ValidationType::SumValidation);

        CHECK(config.scaling_factors().size() == 4);
    }

    SUBCASE("valid file no report year")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = "C:/emap/emissies/"
                type = "gains"
                year = 2020
                scenario = "scenarionaam"
                scalefactors = "{}" 

            [output]
                path = "c:/temp"

            [options]
                validation = true
        )toml";

        const auto config = parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string())));

        CHECK(config.grid_definition() == GridDefinition::Beleuros);
        CHECK(config.data_root() == fs::path("C:/emap/emissies/"));
        CHECK(config.run_type() == RunType::Gains);
        CHECK(config.year() == 2020_y);
        CHECK(!config.reporting_year().has_value());
        CHECK(config.scenario() == "scenarionaam");

        CHECK(config.output_path() == fs::path("c:/temp"));
        CHECK(config.validation_type() == ValidationType::SumValidation);
    }

    SUBCASE("invalid file: empty")
    {
        CHECK_THROWS_WITH_AS(parse_run_configuration(std::string_view()), "No 'input' section present in configuration", RuntimeError);
    }

    SUBCASE("invalid file: missing output section")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = "C:/emap/emissies/"
                type = "gains"
                year = 2020
                scenario = "scenarionaam"
                scalefactors = "{}" 

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string()))),
                             "No 'output' section present in configuration",
                             RuntimeError);
    }

    SUBCASE("invalid file: no path quotes")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = C:/emap/emissies/
                type = "gains"
                year = 2020
                scenario = "scenarionaam"
                scalefactors = "{}"

            [output]
                path = "c:/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_AS(parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string()))), RuntimeError);
    }

    SUBCASE("invalid file: year as string")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = "C:/emap/emissies/"
                type = "gains"
                year = "2020"
                scenario = "scenarionaam"
                scalefactors = "{}" 

            [output]
                path = "c:/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string()))), "Invalid year present in 'input' section, year values should not be quoted (e.g. year = 2020)", RuntimeError);
    }

    SUBCASE("invalid file: scenario is integer")
    {
        std::string_view tomlConfig = R"toml(
            [input]
                grid = "beleuros"
                datapath = "C:/emap/emissies/"
                type = "gains"
                year = 2020
                scenario = 2010
                scalefactors = "{}" 

            [output]
                path = "c:/temp"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(std::string_view(fmt::format(tomlConfig, scaleFactors.generic_u8string()))), "'scenario' key value in 'input' section should be a quoted string (e.g. scenario = \"value\")", RuntimeError);
    }
}

}
