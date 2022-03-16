#include "emap/outputbuilderfactory.h"

#include "chimereoutputbuilder.h"
#include "emap/runconfiguration.h"
#include "vlopsoutputbuilder.h"

#include "infra/gdal.h"
#include "infra/log.h"

#include <unordered_map>

namespace emap {

using namespace inf;

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

static std::unordered_map<std::string, SectorParameterConfig> parse_sector_parameters_config(const fs::path& diffuseParametersPath, SectorLevel level, std::string_view outputSectorLevelName)
{
    std::unordered_map<std::string, SectorParameterConfig> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(diffuseParametersPath);
    auto layer = ds.layer(layer_name_for_sector_level(level, outputSectorLevelName));

    const auto colSector = layer.layer_definition().required_field_index("Sector");
    const auto colHc     = layer.layer_definition().required_field_index("hc(MW)");
    const auto colH      = layer.layer_definition().required_field_index("h(m)");
    const auto colS      = layer.layer_definition().required_field_index("s(m)");
    const auto colTb     = layer.layer_definition().required_field_index("tb");
    const auto colId     = layer.layer_definition().required_field_index("Id");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            break; // abort on empty lines
        }

        SectorParameterConfig config;
        config.hc_MW = feature.field_as<double>(colHc);
        config.h_m   = feature.field_as<double>(colH);
        config.s_m   = feature.field_as<double>(colS);
        config.tb    = feature.field_as<double>(colTb);
        config.id    = feature.field_as<int32_t>(colId);

        result.emplace(feature.field_as<std::string_view>(colSector), config);
    }

    return result;
}

static std::unordered_map<CountryId, int32_t> parse_chimere_country_mapping(const fs::path& mappingPath, const CountryInventory& countryInv)
{
    std::unordered_map<CountryId, int32_t> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(mappingPath);
    auto layer = ds.layer(0);

    const auto colIso     = layer.layer_definition().required_field_index("country_iso_code");
    const auto colChimere = layer.layer_definition().required_field_index("Chimere_country");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            break; // skip empty lines
        }

        if (auto country = countryInv.try_country_from_string(feature.field_as<std::string_view>(colIso)); country.has_value()) {
            result.emplace(country->id(), feature.field_as<int32_t>(colChimere));
        } else {
            Log::warn("Unknown country in chimere mapping file: {}", feature.field_as<std::string_view>(colIso));
        }
    }

    return result;
}

std::unordered_map<std::string, VlopsOutputBuilder::PollutantParameterConfig> parse_pollutant_parameters_config(const fs::path& pollutantParametersPath, const PollutantInventory& pollutants)
{
    std::unordered_map<std::string, VlopsOutputBuilder::PollutantParameterConfig> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(pollutantParametersPath);

    auto layer = ds.layer("sd");

    const auto colPollutant = layer.layer_definition().required_field_index("pollutant_code");
    const auto colSd        = layer.layer_definition().required_field_index("sd");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            continue; // skip empty lines
        }

        VlopsOutputBuilder::PollutantParameterConfig config;
        config.sd = feature.field_as<int32_t>(colSd);

        if (auto pollutant = pollutants.try_pollutant_from_string(feature.field_as<std::string_view>(colPollutant)); pollutant.has_value()) {
            result.emplace(pollutant->code(), config);
        } else {
            Log::warn("Unknown pollutant in parameters config: {}", feature.field_as<std::string_view>(colPollutant));
        }
    }

    return result;
}

std::unique_ptr<IOutputBuilder> make_output_builder(const RunConfiguration& cfg)
{
    auto modelGrid                  = cfg.model_grid();
    const auto sectorParametersPath = cfg.data_root() / "05_model_parameters" / "sector_parameters.xlsx";
    auto sectorParams               = parse_sector_parameters_config(sectorParametersPath, cfg.output_sector_level(), cfg.output_sector_level_name());

    if (modelGrid == ModelGrid::Vlops1km || modelGrid == ModelGrid::Vlops250m) {
        const auto pollutantParametersPath = cfg.data_root() / "05_model_parameters" / "pollutant_parameters.xlsx";

        auto pollutantParams = parse_pollutant_parameters_config(pollutantParametersPath, cfg.pollutants());

        return std::make_unique<VlopsOutputBuilder>(std::move(sectorParams), std::move(pollutantParams), cfg);
    } else if (modelGrid == ModelGrid::Chimere05deg ||
               modelGrid == ModelGrid::Chimere01deg ||
               modelGrid == ModelGrid::Chimere005degLarge ||
               modelGrid == ModelGrid::Chimere005degSmall ||
               modelGrid == ModelGrid::Chimere0025deg ||
               modelGrid == ModelGrid::ChimereEmep ||
               modelGrid == ModelGrid::ChimereCams) {
        const auto countryMappingPath = cfg.data_root() / "05_model_parameters" / "chimere_mapping_country.xlsx";
        auto countryMapping           = parse_chimere_country_mapping(countryMappingPath, cfg.countries());
        return std::make_unique<ChimereOutputBuilder>(std::move(sectorParams), std::move(countryMapping), cfg);
    }

    throw RuntimeError("No known output builder for the specified grid definition");
}
}