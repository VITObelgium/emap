#include "emap/configurationparser.h"

#include "infra/cast.h"
#include "infra/exception.h"
#include "infra/gdal.h"

#include <cassert>
#include <filesystem>
#include <toml++/toml.h>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

static EmissionDestination emission_destination_from_string(std::string_view str)
{
    if (str == "land") {
        return EmissionDestination::Land;
    }

    if (str == "sea") {
        return EmissionDestination::Sea;
    }

    throw RuntimeError("Invalid emission destination type: {}", str);
}

CountryInventory parse_countries(const fs::path& countriesSpec)
{
    std::vector<Country> countries;
    InputConversions conversions;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(countriesSpec);

    {
        auto layer = ds.layer("country");

        const auto colIsoCode = layer.layer_definition().required_field_index("country_iso_code");
        const auto colLabel   = layer.layer_definition().required_field_index("country_label");
        const auto colType    = layer.layer_definition().required_field_index("type");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            countries.emplace_back(feature.field_as<std::string_view>(colIsoCode),
                                   feature.field_as<std::string_view>(colLabel),
                                   feature.field_as<std::string_view>(colType) == "land");
        }
    }

    return CountryInventory(std::move(countries));
}

SectorInventory parse_sectors(const fs::path& sectorSpec, const fs::path& conversionSpec)
{
    std::vector<GnfrSector> gnfrSectors;
    std::vector<NfrSector> nfrSectors;

    InputConversions gnfrConversions;
    InputConversions nfrConversions;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(sectorSpec);

    {
        auto layer = ds.layer("GNFR");

        const auto colNumber = layer.layer_definition().required_field_index("GNFR_number");
        const auto colLabel  = layer.layer_definition().required_field_index("GNFR_label");
        const auto colCode   = layer.layer_definition().required_field_index("GNFR_code");
        const auto colType   = layer.layer_definition().required_field_index("type");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            gnfrSectors.emplace_back(feature.field_as<std::string_view>(colLabel),
                                     GnfrId(feature.field_as<int64_t>(colNumber)),
                                     feature.field_as<std::string_view>(colCode),
                                     "",
                                     emission_destination_from_string(feature.field_as<std::string_view>(colType)));
        }
    }

    {
        auto layer = ds.layer("NFR");

        const auto colCode        = layer.layer_definition().required_field_index("NFR_code");
        const auto colNumber      = layer.layer_definition().required_field_index("NFR_number");
        const auto colDescription = layer.layer_definition().required_field_index("NFR_description");
        const auto colType        = layer.layer_definition().required_field_index("type");
        const auto colGnfr        = layer.layer_definition().required_field_index("GNFR");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            const auto nfrCode  = feature.field_as<std::string_view>(colCode);
            const auto gnfrName = feature.field_as<std::string_view>(colGnfr);

            const auto* gnfrSector = find_in_container(gnfrSectors, [=](const GnfrSector& sector) {
                return sector.name() == gnfrName;
            });

            if (gnfrSector == nullptr) {
                throw RuntimeError("Invalid GNFR sector ('{}') configured for NFR sector '{}'", feature.field_as<std::string_view>(colGnfr), nfrCode);
            }

            const auto destination = emission_destination_from_string(feature.field_as<std::string_view>(colType));

            nfrSectors.emplace_back(nfrCode, NfrId(feature.field_as<int64_t>(colNumber)), *gnfrSector, feature.field_as<std::string_view>(colDescription), destination);
        }
    }

    {
        auto ds = gdal::VectorDataSet::open(conversionSpec);

        {
            auto layer = ds.layer("gnfr");

            const auto colCode = layer.layer_definition().required_field_index("GNFR_code");
            const auto colName = layer.layer_definition().required_field_index("GNFR_names");

            for (const auto& feature : layer) {
                if (!feature.field_is_valid(0)) {
                    continue; // skip empty lines
                }

                gnfrConversions.add_conversion(feature.field_as<std::string_view>(colCode), feature.field_as<std::string_view>(colName), {});
            }
        }
    }

    {
        auto ds = gdal::VectorDataSet::open(conversionSpec);

        {
            auto layer = ds.layer("nfr");

            const auto colCode     = layer.layer_definition().required_field_index("NFR_code");
            const auto colName     = layer.layer_definition().required_field_index("NFR_names");
            const auto colPriority = layer.layer_definition().required_field_index("NFR_priority");

            for (const auto& feature : layer) {
                if (!feature.field_is_valid(0)) {
                    continue; // skip empty lines
                }

                nfrConversions.add_conversion(feature.field_as<std::string_view>(colCode), feature.field_as<std::string_view>(colName), feature.field_as<int32_t>(colPriority));
            }
        }
    }

    return SectorInventory(std::move(gnfrSectors), std::move(nfrSectors), std::move(gnfrConversions), std::move(nfrConversions));
}

PollutantInventory parse_pollutants(const fs::path& pollutantSpec, const fs::path& conversionSpec)
{
    std::vector<Pollutant> pollutants;
    InputConversions conversions;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");

    {
        auto ds = gdal::VectorDataSet::open(pollutantSpec);

        {
            auto layer = ds.layer("pollutant");

            const auto colCode  = layer.layer_definition().required_field_index("pollutant_code");
            const auto colLabel = layer.layer_definition().required_field_index("pollutant_label");

            for (const auto& feature : layer) {
                if (!feature.field_is_valid(0)) {
                    continue; // skip empty lines
                }

                pollutants.emplace_back(feature.field_as<std::string_view>(colCode), feature.field_as<std::string_view>(colLabel));
            }
        }
    }

    {
        auto ds = gdal::VectorDataSet::open(conversionSpec);

        {
            auto layer = ds.layer("pollutant");

            const auto colCode = layer.layer_definition().required_field_index("pollutant_code");
            const auto colName = layer.layer_definition().required_field_index("pollutant_names");

            for (const auto& feature : layer) {
                if (!feature.field_is_valid(0)) {
                    continue; // skip empty lines
                }

                conversions.add_conversion(feature.field_as<std::string_view>(colCode), feature.field_as<std::string_view>(colName), {});
            }
        }
    }

    return PollutantInventory(std::move(pollutants), std::move(conversions));
}

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

    if (grid == "vlops1km") {
        return GridDefinition::Vlops1km;
    }

    if (grid == "vlops250m") {
        return GridDefinition::Vlops250m;
    }

    if (grid == "rio4x4") {
        return GridDefinition::Rio4x4;
    }

    if (grid == "rio4x4extended") {
        return GridDefinition::Rio4x4Extended;
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
            result = fs::absolute(basePath / result);
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

static std::optional<RunConfiguration> parse_run_configuration_impl(std::string_view configContents, const fs::path& tomlPath)
{
    try {
        const auto basePath     = tomlPath.parent_path();
        const toml::table table = toml::parse(configContents, tomlPath.u8string());

        if (!table.contains("model")) {
            // No model run configured
            return {};
        }

        NamedSection model("model", table["model"]);

        const auto grid       = read_grid(model.section["grid"].value<std::string_view>());
        const auto dataPath   = read_path(model, "datapath", basePath);
        const auto runType    = read_run_type(model.section["type"].value<std::string_view>());
        const auto year       = read_year(model.section["year"]);
        const auto reportYear = read_year(model.section["report_year"]);
        const auto scenario   = read_string(model, "scenario");
        const auto outputPath = read_path(model, "output", basePath);

        auto sectorInventory    = parse_sectors(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx", dataPath / "05_model_parameters" / "code_conversions.xlsx");
        auto pollutantInventory = parse_pollutants(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx", dataPath / "05_model_parameters" / "code_conversions.xlsx");
        auto countryInventory   = parse_countries(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx");

        const auto optionsSection = table["options"];
        bool validate             = optionsSection["validation"].value_or<bool>(false);

        return RunConfiguration(dataPath,
                                fs::path(), //countriesVectorPath,
                                grid,
                                runType,
                                validate ? ValidationType::SumValidation : ValidationType::NoValidation,
                                year,
                                reportYear,
                                scenario,
                                std::move(sectorInventory),
                                std::move(pollutantInventory),
                                std::move(countryInventory),
                                outputPath);
    } catch (const toml::parse_error& e) {
        if (const auto& errorBegin = e.source().begin; errorBegin) {
            throw RuntimeError("Failed to parse run configuration: {} (line {} column {})", e.description(), errorBegin.line, errorBegin.column);
        }

        throw RuntimeError("Failed to parse run configuration: {}", e.description());
    }
}

std::optional<RunConfiguration> parse_run_configuration_file(const fs::path& config)
{
    return parse_run_configuration_impl(file::read_as_text(config), config);
}

std::optional<RunConfiguration> parse_run_configuration(std::string_view configContents, const fs::path& basePath)
{
    return parse_run_configuration_impl(configContents, basePath / "dummy.toml");
}
}
