﻿#include "emap/configurationparser.h"

#include "infra/cast.h"
#include "infra/exception.h"
#include "infra/gdal.h"
#include "infra/log.h"
#include "infra/string.h"

#include <cassert>
#include <filesystem>
#include <toml++/toml.h>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;
namespace gdal = inf::gdal;

static EmissionDestination emission_destination_from_string(std::string_view str)
{
    if (str::iequals(str, "land")) {
        return EmissionDestination::Land;
    }

    if (str::iequals(str, "sea")) {
        return EmissionDestination::Sea;
    }

    if (str::iequals(str, "eez")) {
        return EmissionDestination::Eez;
    }

    throw RuntimeError("Invalid emission destination type: {}", str);
}

static std::string layer_name_for_sector_level(SectorLevel level, std::string_view outputSectorLevelName)
{
    switch (level) {
    case SectorLevel::GNFR:
        return "gnfr";
    case SectorLevel::NFR:
        return "nfr";
    case SectorLevel::Custom:
        return str::lowercase(outputSectorLevelName);
    }

    throw RuntimeError("Invalid sector level");
}

SectorParameterConfiguration parse_sector_parameters_config(const fs::path& diffuseParametersPath,
                                                            SectorLevel level,
                                                            const PollutantInventory& polInv,
                                                            std::string_view outputSectorLevelName)
{
    SectorParameterConfiguration result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(diffuseParametersPath);
    auto layer = ds.layer(layer_name_for_sector_level(level, outputSectorLevelName));

    const auto colPollutant = layer.layer_definition().required_field_index("Pollutant");
    const auto colSector    = layer.layer_definition().required_field_index("Sector");
    const auto colHc        = layer.layer_definition().required_field_index("hc(MW)");
    const auto colH         = layer.layer_definition().required_field_index("h(m)");
    const auto colS         = layer.layer_definition().required_field_index("s(m)");
    const auto colTb        = layer.layer_definition().required_field_index("tb");
    const auto colId        = layer.layer_definition().required_field_index("Id");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            break; // abort on empty lines
        }

        SectorParameters config;
        config.hc_MW = feature.field_as<double>(colHc);
        config.h_m   = feature.field_as<double>(colH);
        config.s_m   = feature.field_as<double>(colS);
        config.tb    = feature.field_as<double>(colTb);
        config.id    = feature.field_as<int32_t>(colId);

        std::string sectorName(feature.field_as<std::string_view>(colSector));
        auto pollutantName = feature.field_as<std::string_view>(colPollutant);
        if (str::trimmed_view(pollutantName) == "*") {
            result.add_parameter(sectorName, config);
        } else {
            result.add_pollutant_specific_parameter(sectorName, polInv.pollutant_from_string(pollutantName), config);
        }
    }

    return result;
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

static std::vector<IgnoredName> parse_ignore_list(const fs::path& ignoreSpec, const std::string& tab, const CountryInventory& countries)
{
    std::vector<IgnoredName> ignored;

    if (fs::is_regular_file(ignoreSpec)) {
        CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
        auto ds    = gdal::VectorDataSet::open(ignoreSpec);
        auto layer = ds.layer(tab);

        auto colExceptions = layer.layer_definition().field_index("country_exceptions");

        // When no ignores are present, gdal gets confused and does not recognise the header
        if (const auto colName = layer.layer_definition().field_index("names"); colName >= 0) {
            for (const auto& feature : layer) {
                if (!feature.field_is_valid(0)) {
                    continue; // skip empty lines
                }

                std::unordered_set<CountryId> countryExceptions;
                if (colExceptions >= 0) {
                    auto ignoredCountries = str::trimmed_view(feature.field_as<std::string_view>(colExceptions));
                    if (!ignoredCountries.empty()) {
                        for (auto country : str::split_view(ignoredCountries, ';')) {
                            countryExceptions.emplace(countries.country_from_string(country).id());
                        }
                    }
                }

                ignored.emplace_back(IgnoredName(feature.field_as<std::string_view>(colName), countryExceptions));
            }
        }
    }

    return ignored;
}

SectorInventory parse_sectors(const fs::path& sectorSpec,
                              const fs::path& conversionSpec,
                              const fs::path& ignoreSpec,
                              const CountryInventory& countries)
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

    auto ignoredNfrSectors  = parse_ignore_list(ignoreSpec, "nfr", countries);
    auto ignoredGnfrSectors = parse_ignore_list(ignoreSpec, "gnfr", countries);

    return SectorInventory(std::move(gnfrSectors), std::move(nfrSectors),
                           std::move(gnfrConversions), std::move(nfrConversions),
                           std::move(ignoredGnfrSectors), std::move(ignoredNfrSectors));
}

PollutantInventory parse_pollutants(const fs::path& pollutantSpec,
                                    const fs::path& conversionSpec,
                                    const fs::path& ignoreSpec,
                                    const CountryInventory& countries)
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

    return PollutantInventory(std::move(pollutants), std::move(conversions), parse_ignore_list(ignoreSpec, "pollutant", countries));
}

std::unordered_map<NfrId, std::string> parse_sector_mapping(const fs::path& mappingSpec, const SectorInventory& inv, const std::string& outputLevel)
{
    std::unordered_map<NfrId, std::string> result;

    // No mapping needed when output level is NFR
    if (!str::iequals(outputLevel, "NFR")) {
        CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
        auto ds    = gdal::VectorDataSet::open(mappingSpec);
        auto layer = ds.layer(0);

        const auto colNfr    = layer.layer_definition().required_field_index("NFR_code");
        const auto colMapped = layer.layer_definition().required_field_index(outputLevel);

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(colNfr)) {
                continue; // skip empty lines
            }

            auto nfrSector = inv.try_nfr_sector_from_string(feature.field_as<std::string_view>(colNfr));
            if (nfrSector.has_value()) {
                result.emplace(nfrSector->id(), feature.field_as<std::string_view>(colMapped));
            } else {
                Log::warn("Unknown nfr id present in mapping file: {}", feature.field_as<std::string_view>(colNfr));
            }
        }
    }

    return result;
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
    auto gridLowercase = str::lowercase(grid);

    if (gridLowercase == "vlops1km") {
        return ModelGrid::Vlops1km;
    }

    if (gridLowercase == "vlops250m") {
        return ModelGrid::Vlops250m;
    }

    if (gridLowercase == "chimere_05deg") {
        return ModelGrid::Chimere05deg;
    }

    if (gridLowercase == "chimere_01deg") {
        return ModelGrid::Chimere01deg;
    }

    if (gridLowercase == "chimere_005deg_large") {
        return ModelGrid::Chimere005degLarge;
    }

    if (gridLowercase == "chimere_005deg_small") {
        return ModelGrid::Chimere005degSmall;
    }

    if (gridLowercase == "chimere_0025deg") {
        return ModelGrid::Chimere0025deg;
    }

    if (gridLowercase == "chimere_emep_01deg") {
        return ModelGrid::ChimereEmep;
    }

    if (gridLowercase == "chimere_cams_01-005deg") {
        return ModelGrid::ChimereCams;
    }

    if (gridLowercase == "chimere_rio1") {
        return ModelGrid::ChimereRio1;
    }

    if (gridLowercase == "chimere_rio4") {
        return ModelGrid::ChimereRio4;
    }

    if (gridLowercase == "chimere_rio32") {
        return ModelGrid::ChimereRio32;
    }

    if (gridLowercase == "sherpa_emep") {
        return ModelGrid::SherpaEmep;
    }

    if (gridLowercase == "sherpa_chimere") {
        return ModelGrid::SherpaChimere;
    }

    if (gridLowercase == "quark_1km") {
        return ModelGrid::Quark1km;
    }

    throw RuntimeError("Invalid model grid type: '{}'", grid);
}

static ModelGrid read_grid(std::optional<std::string_view> grid)
{
    if (!grid.has_value()) {
        throw RuntimeError("No grid definition present in 'model' section (e.g. grid = \"beleuros\")");
    }

    return model_grid_from_string(*grid);
}

static std::string read_sector_level(std::optional<std::string_view> level)
{
    if (!level.has_value()) {
        throw RuntimeError("No sector level present in 'output' section (e.g. sector_level = \"GNFR\")");
    }

    return std::string(*level);
}

static fs::path read_optional_path(const NamedSection& ns, std::string_view name, const fs::path& basePath)
{
    assert(ns.section.is_table());
    auto nodeValue = ns.section[name];

    if (!nodeValue) {
        return {};
    }

    if (auto pathValue = nodeValue.value<std::string_view>(); pathValue.has_value()) {
        auto result = file::u8path(*pathValue);
        if ((!result.empty()) && result.is_relative()) {
            result = fs::absolute(basePath / result);
        }

        return result;
    } else {
        throw RuntimeError("Invalid path value for '{0:}' key in '{1:}' section (e.g. {0:} = \"/some/path\")", name, ns.name);
    }
}

static fs::path read_path(const NamedSection& ns, std::string_view name, const fs::path& basePath)
{
    auto path = read_optional_path(ns, name, basePath);
    if (path.empty()) {
        throw RuntimeError("'{0:}' key not present in '{1:}' section (e.g. {0:} = \"/some/path\")", name, ns.name);
    }

    return path;
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

static std::vector<Pollutant> read_pollutants(toml::node_view<const toml::node> nodeValue, const PollutantInventory& inv)
{
    std::vector<Pollutant> result;

    if (nodeValue) {
        if (const toml::array* arr = nodeValue.as_array()) {
            for (const toml::node& elem : *arr) {
                if (auto pollutantName = elem.value<std::string>(); pollutantName.has_value()) {
                    result.push_back(inv.pollutant_from_string(*pollutantName));
                }
            }
        }
    }

    return result;
}

// static std::string read_string(const NamedSection& ns, std::string_view name)
// {
//     assert(ns.section.is_table());
//     auto nodeValue = ns.section[name];

//     if (!nodeValue) {
//         throw RuntimeError("'{}' key not present in {} section", name, ns.name);
//     }

//     if (!nodeValue.is_string()) {
//         throw RuntimeError("'{0:}' key value in '{1:}' section should be a quoted string (e.g. {0:} = \"value\")", name, ns.name);
//     }

//     assert(nodeValue.value<std::string>().has_value());
//     return nodeValue.value<std::string>().value();
// }

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
        const toml::table table = toml::parse(configContents, str::from_u8(tomlPath.u8string()));
        if (!table.contains("model")) {
            throw RuntimeError("No model section present in configuration file");
        }

        if (!table.contains("output")) {
            throw RuntimeError("No output section present in configuration file");
        }

        NamedSection model("model", table["model"]);
        NamedSection output("output", table["output"]);

        const auto dataPath = read_path(model, "datapath", basePath);

        const auto parametersPath             = basePath / dataPath / "05_model_parameters";
        const auto idNumbersPath              = parametersPath / "id_nummers.xlsx";
        const auto codeConversionsNumbersPath = parametersPath / "code_conversions.xlsx";
        const auto ignorePath                 = parametersPath / "names_to_be_ignored.xlsx";

        auto countryInventory   = parse_countries(idNumbersPath);
        auto sectorInventory    = parse_sectors(idNumbersPath, codeConversionsNumbersPath, ignorePath, countryInventory);
        auto pollutantInventory = parse_pollutants(idNumbersPath, codeConversionsNumbersPath, ignorePath, countryInventory);

        const auto grid                         = read_grid(model.section["grid"].value<std::string_view>());
        const auto scenario                     = read_string(model, "scenario", "");
        const auto combinePointSources          = model.section["combine_identical_point_sources"].value<bool>().value_or(true);
        const double rescaleThreshold           = model.section["point_source_rescale_threshold"].value<double>().value_or(100.0);
        const auto year                         = read_year(model.section["year"]);
        const auto reportYear                   = read_year(model.section["report_year"]);
        const auto spatialPatternExceptionsPath = read_optional_path(model, "spatial_pattern_exceptions", basePath);
        const auto emissionScalingsPath         = read_optional_path(model, "emission_scaling_factors", basePath);
        const auto boundariesPath               = file::u8path(read_string(model, "spatial_boundaries_filename", {}));
        const auto boundariesEezPath            = file::u8path(read_string(model, "spatial_boundaries_eez_filename", {}));
        auto includedPollutants                 = read_pollutants(model.section["included_pollutants"], pollutantInventory);

        RunConfiguration::Output outputConfig;

        outputConfig.path                        = read_path(output, "path", basePath);
        outputConfig.outputLevelName             = read_sector_level(output.section["sector_level"].value<std::string_view>());
        outputConfig.filenameSuffix              = read_string(output, "filename_suffix", "");
        outputConfig.separatePointSources        = output.section["separate_point_sources"].value<bool>().value_or(true);
        outputConfig.createCountryRasters        = output.section["create_country_rasters"].value<bool>().value_or(false);
        outputConfig.createGridRasters           = output.section["create_grid_rasters"].value<bool>().value_or(false);
        outputConfig.createSpatialPatternRasters = output.section["create_spatial_pattern_rasters"].value<bool>().value_or(false);

        parse_missing_pollutant_references(basePath / dataPath / "03_spatial_disaggregation" / "pollutant_reference_when_missing.xlsx", pollutantInventory);
        sectorInventory.set_output_mapping(parse_sector_mapping(parametersPath / "mapping_sectors.xlsx", sectorInventory, outputConfig.outputLevelName));

        const auto optionsSection = table["options"];
        bool validate             = optionsSection["validation"].value_or<bool>(false);

        return RunConfiguration(dataPath,
                                spatialPatternExceptionsPath,
                                emissionScalingsPath,
                                boundariesPath,
                                boundariesEezPath,
                                grid,
                                validate ? ValidationType::SumValidation : ValidationType::NoValidation,
                                year,
                                reportYear,
                                scenario,
                                combinePointSources,
                                rescaleThreshold,
                                std::move(includedPollutants),
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
    if (config.empty()) {
        throw RuntimeError("No config file provided");
    }

    return parse_run_configuration_impl(file::read_as_text(config), config);
}

RunConfiguration parse_run_configuration(std::string_view configContents, const fs::path& basePath)
{
    return parse_run_configuration_impl(configContents, basePath / "dummy.toml");
}
}
