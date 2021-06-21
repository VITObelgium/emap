#include "emap/runconfigurationparser.h"
#include "emap/inputparsers.h"
#include "emap/runconfiguration.h"

#include "infra/cast.h"
#include "infra/exception.h"

#include <cassert>
#include <filesystem>
#include <toml++/toml.h>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

struct NamedSection
{
    NamedSection(std::string_view theName, toml::node_view<const toml::node> theSection)
    : name(theName)
    , section(theSection)
    {
    }

    std::string name;
    toml::node_view<const toml::node> section;
};

static GridDefinition grid_from_string(std::string_view grid)
{
    if (grid == "beleuros") {
        return GridDefinition::Beleuros;
    }

    if (grid == "chimere1") {
        return GridDefinition::Chimere1;
    }

    throw RuntimeError("Invalid grid type: '{}'", grid);
}

static RunType run_type_from_string(std::string_view type)
{
    if (type == "emep") {
        return RunType::Emep;
    }

    if (type == "gains") {
        return RunType::Gains;
    }

    throw RuntimeError("Invalid run type: '{}'", type);
}

static GridDefinition read_grid(std::optional<std::string_view> grid)
{
    if (!grid.has_value()) {
        throw RuntimeError("No grid definition present in 'input' section (e.g. grid = \"beleuros\")");
    }

    return grid_from_string(*grid);
}

static RunType read_run_type(std::optional<std::string_view> type)
{
    if (!type.has_value()) {
        throw RuntimeError("No type present in 'input' section (e.g. type = \"gains\")");
    }

    return run_type_from_string(*type);
}

static fs::path read_path(const NamedSection& ns, std::string_view name, const fs::path& basePath)
{
    assert(ns.section.is_table());
    auto nodeValue = ns.section[name];

    if (!nodeValue) {
        throw RuntimeError("'{0:}' key not present in '{1:}' section (e.g. {0:} = \"/some/path\")", name, ns.name);
    }

    if (auto pathValue = nodeValue.value<std::string_view>(); pathValue.has_value()) {
        auto result = fs::u8path(*pathValue);
        if (result.is_relative()) {
            result = basePath / result;
            if (fs::exists(result)) {
                result = fs::canonical(basePath / result);
            }
        }

        return result;
    } else {
        throw RuntimeError("Invalid path value for '{0:}' key in '{1:}' section (e.g. {0:} = \"/some/path\")", name, ns.name);
    }
}

static date::year parse_year(toml::node_view<const toml::node> nodeValue)
{
    assert(nodeValue);

    if (!nodeValue.is_integer()) {
        if (nodeValue.is_string()) {
            throw RuntimeError("Invalid year present in 'input' section, year values should not be quoted (e.g. year = 2020)");
        }

        throw RuntimeError("Invalid year present in 'input' section ({})", nodeValue.value_or<std::string_view>(""sv));
    }

    auto yearInt = nodeValue.value<int64_t>();
    if (!yearInt.has_value()) {
        throw RuntimeError("Invalid year present in 'input' section ({})", nodeValue.value_or<std::string_view>(""sv));
    }

    date::year result(truncate<int32_t>(*yearInt));
    if (!fits_in_type<int32_t>(*yearInt) || !result.ok()) {
        throw RuntimeError("Invalid year value present in 'input' section ({})", *yearInt);
    }

    return result;
}

static date::year read_year(toml::node_view<const toml::node> nodeValue)
{
    if (!nodeValue) {
        throw RuntimeError("No year present in 'input' section (e.g. year = 2020)");
    }

    return parse_year(nodeValue);
}

static std::optional<date::year> read_optional_year(toml::node_view<const toml::node> nodeValue)
{
    if (!nodeValue) {
        return {};
    }

    return parse_year(nodeValue);
}

static std::string read_string(const NamedSection& ns, std::string_view name)
{
    assert(ns.section.is_table());
    auto nodeValue = ns.section[name];

    if (!nodeValue) {
        throw RuntimeError("'{}' key not present in {} section", name, ns.name);
    }

    if (!nodeValue.is_string()) {
        throw RuntimeError("'{0:}' key value in '{1:}' section should be a quoted string (e.g. {0:} = \"value\")", name, ns.name);
    }

    assert(nodeValue.value<std::string>().has_value());
    return nodeValue.value<std::string>().value();
}

static void throw_on_missing_section(const toml::table& table, std::string_view name)
{
    if (!table.contains(name)) {
        throw RuntimeError("No '{}' section present in configuration", name);
    }
}

static RunConfiguration parse_run_configuration(std::string_view configContents, const fs::path& tomlPath)
{
    try {
        const auto basePath     = tomlPath.parent_path();
        const toml::table table = toml::parse(configContents, tomlPath.u8string());

        throw_on_missing_section(table, "input");
        throw_on_missing_section(table, "output");

        NamedSection input("input", table["input"]);
        NamedSection output("output", table["output"]);

        const auto grid       = read_grid(input.section["grid"].value<std::string_view>());
        const auto dataPath   = read_path(input, "datapath", basePath);
        const auto runType    = read_run_type(input.section["type"].value<std::string_view>());
        const auto year       = read_year(input.section["year"]);
        const auto reportYear = read_optional_year(input.section["report_year"]);
        const auto scenario   = read_string(input, "scenario");

        const auto outputPath = read_path(output, "path", basePath);

        const auto optionsSection = table["options"];
        bool validate             = optionsSection["validation"].value_or<bool>(false);

        return RunConfiguration(dataPath,
                                grid,
                                runType,
                                validate ? ValidationType::SumValidation : ValidationType::NoValidation,
                                year,
                                reportYear,
                                scenario,
                                outputPath);
    } catch (const toml::parse_error& e) {
        if (const auto& errorBegin = e.source().begin; errorBegin) {
            throw RuntimeError("Failed to parse run configuration: {} (line {} column {})", e.description(), errorBegin.line, errorBegin.column);
        }

        throw RuntimeError("Failed to parse run configuration: {}", e.description());
    }
}

RunConfiguration parse_run_configuration(const fs::path& config)
{
    return parse_run_configuration(file::read_as_text(config), config);
}

RunConfiguration parse_run_configuration(std::string_view configContents)
{
    return parse_run_configuration(configContents, "");
}
}
