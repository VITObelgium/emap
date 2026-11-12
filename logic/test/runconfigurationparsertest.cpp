#include "emap/configurationparser.h"
#include "emap/runconfiguration.h"
#include "infra/algo.h"
#include "infra/crs.h"
#include "infra/exception.h"
#include "testconstants.h"

#include "testconfig.h"

#include <cmath>
#include <limits>
#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Parse run configuration")
{
    const auto scaleFactors = file::u8path(TEST_DATA_DIR) / "_input" / "02_scaling" / "historic" / "1990" / "scaling_diffuse.csv";

    const auto expectedDataRoot = fs::canonical(file::u8path(TEST_DATA_DIR) / "_input");
    const auto expectedOutput   = fs::absolute("/temp");

    SUBCASE("valid file")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                emission_scaling_factors = "{}"
                combine_identical_point_sources = true
                point_source_rescale_threshold = 25
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
        CHECK(config.year() == chrono::year(2020));
        CHECK(config.reporting_year() == chrono::year(2018));
        CHECK(config.scenario() == "scenarionaam");
        CHECK(config.flanders_input_epsg() == RunConfiguration::DefaultFlandersInputEpsg);
        CHECK(config.flanders_input_epsg() == crs::epsg::BelgianLambert2008);
        CHECK(config.combine_identical_point_sources() == true);
        CHECK(config.point_source_rescale_threshold() == 25.0);
        CHECK(config.emission_scalings_path() == scaleFactors);
        CHECK(config.spatial_pattern_path() == expectedDataRoot / "03_spatial_disaggregation");
        CHECK(config.boundaries_vector_path() == expectedDataRoot / "03_spatial_disaggregation" / "boundaries" / "spatial_bounds.geojson");
        CHECK(config.eez_boundaries_vector_path() == expectedDataRoot / "03_spatial_disaggregation" / "boundaries" / "boundaries_incl_EEZ.gpkg");

        CHECK(config.output_path() == expectedOutput);
        CHECK(config.validation_type() == ValidationType::SumValidation);

        CHECK(config.included_pollutants() == container_as_vector(config.pollutants().list()));

        CHECK(config.sectors().nfr_sector_from_string("1A3di(ii)").destination() == EmissionDestination::Eez);
        CHECK(config.sectors().nfr_sector_from_string("1A3di(ii)").has_land_destination());
    }

    SUBCASE("valid configured grid")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "file://grid_definition.toml"
                datapath = "_input"
                year = 2020
                report_year = 2018

            [output]
                path = "/temp"
                sector_level = "GNFR"
        )toml";

        const auto config = parse_run_configuration(tomlConfig, file::u8path(TEST_DATA_DIR));

        CHECK(config.model_grid() == ModelGrid::Config);
        CHECK(config.flanders_input_epsg() == RunConfiguration::DefaultFlandersInputEpsg);
        CHECK(grids_for_model_grid(config.model_grid()) == std::vector<GridDefinition>{GridDefinition::Config});

        const auto& configuredGrid = config.grid_data(GridDefinition::Config);
        CHECK(configuredGrid.type == GridDefinition::Config);
        CHECK(configuredGrid.name == "Configured test grid");
        CHECK(configuredGrid.meta.rows == 47);
        CHECK(configuredGrid.meta.cols == 68);
        CHECK(configuredGrid.meta.xll == -11.0);
        CHECK(configuredGrid.meta.yll == 34.5);
        CHECK(configuredGrid.meta.cellSize.x == 0.5);
        CHECK(configuredGrid.meta.cellSize.y == -0.5);
        REQUIRE(configuredGrid.meta.nodata.has_value());
        CHECK(std::isnan(*configuredGrid.meta.nodata));
        CHECK(configuredGrid.meta.geographic_epsg() == crs::epsg::WGS84);
        CHECK(configuredGrid.outputFormat == ModelOuputFormat::Brn);
        CHECK(configuredGrid.gridResolution.empty());
        CHECK(config.model_output_format() == ModelOuputFormat::Brn);
    }

    SUBCASE("configured DAT grid has a resolution string")
    {
        const auto configuredGrid = parse_grid_definition_file(file::u8path(TEST_DATA_DIR) / "grid_definition_dat.toml");

        CHECK(configuredGrid.outputFormat == ModelOuputFormat::Dat);
        CHECK(configuredGrid.gridResolution == "configured_05deg");
    }

    SUBCASE("configured grid file URI requires a path")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "file://"
                datapath = "_input"
                year = 2020
                report_year = 2018

            [output]
                path = "/temp"
                sector_level = "GNFR"
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, file::u8path(TEST_DATA_DIR)),
                             "No grid definition path present after 'file://' in 'model.grid'",
                             RuntimeError);
    }

    SUBCASE("valid file with specified pollutants")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                year = 2020
                report_year = 2018
                scenario = "scenarionaam"
                emission_scaling_factors = "{}"
                point_source_rescale_threshold = 12.5
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
        CHECK(config.year() == chrono::year(2020));
        CHECK(config.reporting_year() == chrono::year(2018));
        CHECK(config.scenario() == "scenarionaam");
        CHECK(config.combine_identical_point_sources() == false);
        CHECK(config.point_source_rescale_threshold() == 12.5);
        CHECK(config.emission_scalings_path() == scaleFactors);
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
                year = 2022
                report_year = 2021
                scenario = "scen"
                emission_scaling_factors = "{}"
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
        CHECK(config.total_emissions_path_nfr(chrono::year(1990), config.reporting_year()) == expectedDataRoot / "01_data_emissions" / "inventory" / "reporting_2021" / "totals" / "nfr_1990_2021.txt");
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
                year = 2020
                scenario = "scenarionaam"
                emission_scaling_factors = "{}"

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
                year = "2020"
                scenario = "scenarionaam"
                emission_scaling_factors = "{}"

            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR)), "Invalid 'model.year' value, expected an integer", RuntimeError);
    }

    SUBCASE("invalid file: scenario is integer")
    {
        constexpr std::string_view tomlConfig = R"toml(
            [model]
                grid = "vlops1km"
                datapath = "_input"
                year = 2020
                report_year = 2020
                scenario = 2010
                emission_scaling_factors = "{}"

            [output]
                path = "/temp"
                sector_level = "GNFR"

            [options]
                validation = true
        )toml";

        CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, str::from_u8(scaleFactors.generic_u8string())), file::u8path(TEST_DATA_DIR)), "Invalid 'model.scenario' value, expected a quoted string", RuntimeError);
    }
}

TEST_CASE("Parse run configuration without optional values")
{
    for (const auto tomlConfig : {
             R"toml(
                 [model]
                 grid = "vlops1km"
                 datapath = "_input"
                 year = 2020
                 report_year = 2018
                 [output]
                 path = "/temp"
                 sector_level = "GNFR"
             )toml",
             R"toml(
                 model.grid = "vlops1km"
                 model.datapath = "_input"
                 model.year = 2020
                 model.report_year = 2018
                 output.path = "/temp"
                 output.sector_level = "GNFR"
             )toml",
         }) {
        CAPTURE(tomlConfig);
        const auto config = parse_run_configuration(tomlConfig, file::u8path(TEST_DATA_DIR));
        CHECK(config.model_grid() == ModelGrid::Vlops1km);
        CHECK(config.data_root() == fs::canonical(file::u8path(TEST_DATA_DIR) / "_input"));
        CHECK(config.output_path() == fs::absolute("/temp"));
        CHECK(config.year() == chrono::year(2020));
        CHECK(config.reporting_year() == chrono::year(2018));
        CHECK(config.flanders_input_epsg() == RunConfiguration::DefaultFlandersInputEpsg);
        CHECK(config.validation_type() == ValidationType::NoValidation);
        CHECK(config.scenario().empty());
        CHECK(config.combine_identical_point_sources());
        CHECK(config.point_source_rescale_threshold() == 100.0);
        CHECK(config.spatial_pattern_exceptions().empty());
        CHECK(config.emission_scalings_path().empty());
        CHECK(config.included_pollutants() == container_as_vector(config.pollutants().list()));
        CHECK(config.output_filename_suffix().empty());
        CHECK(config.output_point_sources_separately());
        CHECK_FALSE(config.output_country_rasters());
        CHECK_FALSE(config.output_grid_rasters());
        CHECK_FALSE(config.output_spatial_pattern_rasters());
    }
}

TEST_CASE("Run configuration rejects unknown keys before loading input")
{
    SUBCASE("unknown root keys and sections")
    {
        struct TestCase
        {
            std::string_view config;
            std::string_view key;
        };
        for (const auto& test : {
                 TestCase{"[input]", "input"},
                 TestCase{"[modle]", "modle"},
                 TestCase{"grid = \"vlops1km\"", "grid"},
                 TestCase{"validation = true", "validation"},
             }) {
            CAPTURE(test.config);
            CHECK_THROWS_WITH_AS(parse_run_configuration(test.config, {}),
                                 fmt::format("Unknown configuration section or key '{}'", test.key), RuntimeError);
        }
    }

    SUBCASE("unknown section keys and misplaced EPSG")
    {
        struct TestCase
        {
            std::string_view section;
            std::string_view key;
        };
        for (const auto& test : {
                 TestCase{"model", "gird"},
                 TestCase{"model", "type"},
                 TestCase{"model", "scalefactors"},
                 TestCase{"output", "flanders_input_epsg"},
                 TestCase{"output", "unknown"},
                 TestCase{"options", "validate"},
             }) {
            const auto tomlConfig = fmt::format("[{}]\n{} = 31370", test.section, test.key);
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 fmt::format("Unknown configuration key '{}.{}'", test.section, test.key), RuntimeError);
        }
    }

    SUBCASE("Lambert72 override must not repeat the model prefix")
    {
        for (const auto tomlConfig : {
                 "[model]\nmodel.flanders_input_epsg = 31370",
                 "[model]\nflanders_input_epsg = 3812\nmodel.flanders_input_epsg = 31370",
             }) {
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 "Unknown configuration key 'model.model'. Nested tables are not supported in [model]; use unprefixed keys within the section",
                                 RuntimeError);
        }
    }

    SUBCASE("repeated prefixes and explicit nested tables")
    {
        for (const auto section : {"model", "output", "options"}) {
            for (const auto nested : {section, "nested"}) {
                for (const auto& tomlConfig : {
                         fmt::format("[{}]\n{}.unknown = true", section, nested),
                         fmt::format("[{}.{}]\nunknown = true", section, nested),
                     }) {
                    CAPTURE(tomlConfig);
                    CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                         fmt::format("Unknown configuration key '{}.{}'. Nested tables are not supported in [{}]; use unprefixed keys within the section",
                                                     section, nested, section),
                                         RuntimeError);
                }
            }
        }
    }
}

TEST_CASE("Run configuration requires sections to be tables before loading input")
{
    for (const auto section : {"model", "output", "options"}) {
        for (const auto value : {"42", "\"invalid\"", "true", "[]", "[{}]"}) {
            const auto tomlConfig = fmt::format("{} = {}", section, value);
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 fmt::format("Configuration section '{}' must be a table", section), RuntimeError);
        }
        const auto tomlConfig = fmt::format("[[{}]]", section);
        CAPTURE(tomlConfig);
        CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                             fmt::format("Configuration section '{}' must be a table", section), RuntimeError);
    }
}

TEST_CASE("Run configuration validates field types before loading input")
{
    SUBCASE("strings, integers and boolean flags")
    {
        struct TestCase
        {
            std::string_view section;
            std::string_view key;
            std::string_view value;
            std::string_view expected;
        };
        for (const auto& test : {
                 TestCase{"model", "grid", "42", "a quoted string"},
                 TestCase{"model", "datapath", "42", "a quoted string"},
                 TestCase{"model", "scenario", "42", "a quoted string"},
                 TestCase{"model", "spatial_pattern_exceptions", "42", "a quoted string"},
                 TestCase{"model", "emission_scaling_factors", "42", "a quoted string"},
                 TestCase{"model", "spatial_boundaries_filename", "42", "a quoted string"},
                 TestCase{"model", "spatial_boundaries_eez_filename", "42", "a quoted string"},
                 TestCase{"output", "path", "42", "a quoted string"},
                 TestCase{"output", "sector_level", "42", "a quoted string"},
                 TestCase{"output", "filename_suffix", "42", "a quoted string"},
                 TestCase{"model", "year", "\"2020\"", "an integer"},
                 TestCase{"model", "report_year", "2020.0", "an integer"},
                 TestCase{"model", "flanders_input_epsg", "3812.0", "an integer"},
                 TestCase{"model", "combine_identical_point_sources", "\"false\"", "a boolean (true or false, without quotes)"},
                 TestCase{"output", "separate_point_sources", "\"true\"", "a boolean (true or false, without quotes)"},
                 TestCase{"output", "create_country_rasters", "1", "a boolean (true or false, without quotes)"},
                 TestCase{"output", "create_grid_rasters", "0", "a boolean (true or false, without quotes)"},
                 TestCase{"output", "create_spatial_pattern_rasters", "[]", "a boolean (true or false, without quotes)"},
                 TestCase{"options", "validation", "\"false\"", "a boolean (true or false, without quotes)"},
             }) {
            const auto tomlConfig = fmt::format("[{}]\n{} = {}", test.section, test.key, test.value);
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 fmt::format("Invalid '{}.{}' value, expected {}", test.section, test.key, test.expected), RuntimeError);
        }
    }

    SUBCASE("rescale threshold must be finite and numeric")
    {
        for (const auto value : {"\"100\"", "true", "[]", "nan", "+inf", "-inf"}) {
            const auto tomlConfig = fmt::format("[model]\npoint_source_rescale_threshold = {}", value);
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 "Invalid 'model.point_source_rescale_threshold' value, expected a finite number", RuntimeError);
        }
    }

    SUBCASE("included pollutants must be an array containing only strings")
    {
        for (const auto value : {"\"CO\"", "true", "[42]", "[\"CO\", false]", "[[\"CO\"]]", "[{ name = \"CO\" }]"}) {
            const auto tomlConfig = fmt::format("[model]\nincluded_pollutants = {}", value);
            CAPTURE(tomlConfig);
            CHECK_THROWS_WITH_AS(parse_run_configuration(tomlConfig, {}),
                                 "Invalid 'model.included_pollutants' value, expected an array of quoted strings", RuntimeError);
        }
    }
}

TEST_CASE("Parse Flanders input EPSG")
{
    constexpr std::string_view tomlConfig = R"toml(
        [model]
            grid = "{}"
            datapath = "_input"
            year = 2020
            report_year = 2018
            flanders_input_epsg = {}

        [output]
            path = "/temp"
            sector_level = "GNFR"
    )toml";

    SUBCASE("projected metre CRS")
    {
        for (const auto grid : {"vlops1km", "file://grid_definition.toml"}) {
            for (const auto epsg : {crs::epsg::BelgianLambert72, crs::epsg::BelgianLambert2008}) {
                CAPTURE(grid);
                CAPTURE(epsg);
                const auto config = parse_run_configuration(fmt::format(tomlConfig, grid, epsg), file::u8path(TEST_DATA_DIR));
                CHECK(config.flanders_input_epsg() == epsg);
            }
        }
    }

    SUBCASE("invalid types and integer ranges")
    {
        for (const auto value : {"\"3812\"", "3812.0", "true", "[]", "[3812]", "{ value = 3812 }", "1979-05-27", "0", "-1", "-2147483648", "2147483648", "9223372036854775807", "-9223372036854775808"}) {
            CAPTURE(value);
            CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, "vlops1km", value), file::u8path(TEST_DATA_DIR)),
                                 Contains("model.flanders_input_epsg"), RuntimeError);
        }
    }

    SUBCASE("type and range errors precede inventory loading")
    {
        for (const auto value : {"\"3812\"", "2147483648"}) {
            CAPTURE(value);
            CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, "vlops1km", value), {}),
                                 Contains("model.flanders_input_epsg"), RuntimeError);
        }
    }

    SUBCASE("unresolved, geographic and feet CRS")
    {
        for (const auto epsg : {999999, std::numeric_limits<int32_t>::max(), crs::epsg::WGS84, 2263}) {
            CAPTURE(epsg);
            CHECK_THROWS_WITH_AS(parse_run_configuration(fmt::format(tomlConfig, "vlops1km", epsg), file::u8path(TEST_DATA_DIR)),
                                 Contains(fmt::format("'model.flanders_input_epsg' value {}", epsg)), RuntimeError);
        }
    }
}

TEST_CASE("Run configuration validates Flanders input EPSG")
{
    SUBCASE("default constructor argument")
    {
        const auto config = RunConfiguration("./data", {}, {}, {}, {}, ModelGrid::Vlops1km, ValidationType::NoValidation,
                                             chrono::year(2020), chrono::year(2018), "", true, 100.0, {},
                                             SectorInventory({}, {}, {}, {}, {}, {}), PollutantInventory({}, {}, {}), CountryInventory({}), {});
        CHECK(config.flanders_input_epsg() == RunConfiguration::DefaultFlandersInputEpsg);
        CHECK(config.flanders_input_epsg() == crs::epsg::BelgianLambert2008);
    }

    const auto createConfig = [](int32_t epsg) {
        return RunConfiguration("./data", {}, {}, {}, {}, ModelGrid::Vlops1km, ValidationType::NoValidation,
                                chrono::year(2020), chrono::year(2018), "", true, 100.0, {},
                                SectorInventory({}, {}, {}, {}, {}, {}), PollutantInventory({}, {}, {}), CountryInventory({}), {},
                                std::nullopt, epsg);
    };

    SUBCASE("explicit projected metre CRS")
    {
        for (const auto epsg : {crs::epsg::BelgianLambert72, crs::epsg::BelgianLambert2008, crs::epsg::WGS84WebMercator}) {
            CAPTURE(epsg);
            CHECK(createConfig(epsg).flanders_input_epsg() == epsg);
        }
    }

    SUBCASE("invalid EPSG")
    {
        for (const auto epsg : {0, -1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), 999999, crs::epsg::WGS84, 4978, 2263}) {
            CAPTURE(epsg);
            CHECK_THROWS_WITH_AS(createConfig(epsg), Contains(fmt::format("'model.flanders_input_epsg' value {}", epsg)), RuntimeError);
        }
    }
}
}
