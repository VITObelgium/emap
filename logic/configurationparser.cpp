#include "emap/configurationparser.h"

#include "infra/cast.h"
#include "infra/exception.h"
#include "infra/gdal.h"
#include "infra/log.h"

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
        const auto colNumber  = layer.layer_definition().required_field_index("country_number");
        const auto colType    = layer.layer_definition().required_field_index("type");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            countries.emplace_back(CountryId(feature.field_as<int32_t>(colNumber)),
                                   feature.field_as<std::string_view>(colIsoCode),
                                   feature.field_as<std::string_view>(colLabel),
                                   str::iequals(feature.field_as<std::string_view>(colType), "land"));
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
                                     GnfrId(feature.field_as<int32_t>(colNumber)),
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

            nfrSectors.emplace_back(nfrCode, NfrId(feature.field_as<int32_t>(colNumber)), *gnfrSector, feature.field_as<std::string_view>(colDescription), destination);
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

void parse_missing_pollutant_references(const fs::path& path, PollutantInventory& inv)
{
    if (!fs::is_regular_file(path)) {
        return;
    }

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");

    auto ds    = gdal::VectorDataSet::open(path);
    auto layer = ds.layer(0);

    const auto colCode      = layer.layer_definition().required_field_index("pollutant_code");
    const auto colReference = layer.layer_definition().required_field_index("reference_pollutant_code");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            continue; // skip empty lines
        }

        try {
            auto pollutant          = inv.pollutant_from_string(feature.field_as<std::string_view>(colCode));
            auto referencePollutant = inv.pollutant_from_string(feature.field_as<std::string_view>(colReference));

            if (pollutant.code() != referencePollutant.code()) {
                inv.add_fallback_for_pollutant(pollutant, referencePollutant);
            }

        } catch (const std::exception& e) {
            Log::warn("Error parsing pollutant reference when missing: {}", e.what());
        }
    }
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

static ModelGrid model_grid_from_string(std::string_view grid)
{
    if (grid == "vlops1km") {
        return ModelGrid::Vlops1km;
    }

    if (grid == "vlops250m") {
        return ModelGrid::Vlops250m;
    }

    throw RuntimeError("Invalid model grid type: '{}'", grid);
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

static ModelGrid read_grid(std::optional<std::string_view> grid)
{
    if (!grid.has_value()) {
        throw RuntimeError("No grid definition present in 'model' section (e.g. grid = \"beleuros\")");
    }

    return model_grid_from_string(*grid);
}

static RunType read_run_type(std::optional<std::string_view> type)
{
    if (!type.has_value()) {
        throw RuntimeError("No type present in 'model' section (e.g. type = \"gains\")");
    }

    return run_type_from_string(*type);
}

static std::string read_sector_level(std::optional<std::string_view> level)
{
    if (!level.has_value()) {
        throw RuntimeError("No sector level present in 'output' section (e.g. sector_level = \"GNFR\")");
    }

    return std::string(*level);
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

static std::string read_string(const NamedSection& ns, std::string_view name, std::string_view defaultValue)
{
    assert(ns.section.is_table());
    auto nodeValue = ns.section[name];

    if (!nodeValue) {
        return std::string(defaultValue);
    }

    if (!nodeValue.is_string()) {
        throw RuntimeError("'{0:}' key value in '{1:}' section should be a quoted string (e.g. {0:} = \"value\")", name, ns.name);
    }

    assert(nodeValue.value<std::string>().has_value());
    return nodeValue.value<std::string>().value();
}

static RunConfiguration parse_run_configuration_impl(std::string_view configContents, const fs::path& tomlPath)
{
    try {
        const auto basePath     = tomlPath.parent_path();
        const toml::table table = toml::parse(configContents, tomlPath.u8string());
        if (!table.contains("model")) {
            throw RuntimeError("No model section present in configuration file");
        }

        if (!table.contains("output")) {
            throw RuntimeError("No output section present in configuration file");
        }

        NamedSection model("model", table["model"]);
        NamedSection output("output", table["output"]);

        const auto grid       = read_grid(model.section["grid"].value<std::string_view>());
        const auto dataPath   = read_path(model, "datapath", basePath);
        const auto runType    = read_run_type(model.section["type"].value<std::string_view>());
        const auto year       = read_year(model.section["year"]);
        const auto reportYear = read_year(model.section["report_year"]);
        const auto scenario   = read_string(model, "scenario");

        RunConfiguration::Output outputConfig;

        outputConfig.path                 = read_path(output, "path", basePath);
        outputConfig.outputLevelName      = read_sector_level(output.section["sector_level"].value<std::string_view>());
        outputConfig.filenameSuffix       = read_string(output, "filename_suffix", "");
        outputConfig.createCountryRasters = output.section["create_country_rasters"].value<bool>().value_or(false);
        outputConfig.createGridRasters    = output.section["create_grid_rasters"].value<bool>().value_or(false);

        auto sectorInventory    = parse_sectors(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx", dataPath / "05_model_parameters" / "code_conversions.xlsx");
        auto pollutantInventory = parse_pollutants(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx", dataPath / "05_model_parameters" / "code_conversions.xlsx");
        auto countryInventory   = parse_countries(basePath / dataPath / "05_model_parameters" / "id_nummers.xlsx");

        parse_missing_pollutant_references(basePath / dataPath / "03_spatial_disaggregation" / "pollutant_reference_when_missing.xlsx", pollutantInventory);

        const auto optionsSection = table["options"];
        bool validate             = optionsSection["validation"].value_or<bool>(false);

        return RunConfiguration(dataPath,
                                grid,
                                runType,
                                validate ? ValidationType::SumValidation : ValidationType::NoValidation,
                                year,
                                reportYear,
                                scenario,
                                std::move(sectorInventory),
                                std::move(pollutantInventory),
                                std::move(countryInventory),
                                outputConfig);
    } catch (const toml::parse_error& e) {
        if (const auto& errorBegin = e.source().begin; errorBegin) {
            throw RuntimeError("Failed to parse run configuration: {} (line {} column {})", e.description(), errorBegin.line, errorBegin.column);
        }

        throw RuntimeError("Failed to parse run configuration: {}", e.description());
    }
}

RunConfiguration parse_run_configuration_file(const fs::path& config)
{
    return parse_run_configuration_impl(file::read_as_text(config), config);
}

RunConfiguration parse_run_configuration(std::string_view configContents, const fs::path& basePath)
{
    return parse_run_configuration_impl(configContents, basePath / "dummy.toml");
}
}
