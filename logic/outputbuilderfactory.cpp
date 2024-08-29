#include "emap/outputbuilderfactory.h"

#include "chimereoutputbuilder.h"
#include "emap/configurationparser.h"
#include "emap/runconfiguration.h"
#include "vlopsoutputbuilder.h"

#include "infra/gdal.h"
#include "infra/log.h"

#include <unordered_map>

namespace emap {

using namespace inf;
namespace gdal = inf::gdal;

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
    const auto sectorParametersPath = cfg.data_root() / "05_model_parameters" / "sector_parameters.xlsx";
    auto sectorParams               = parse_sector_parameters_config(sectorParametersPath, cfg.output_sector_level(), cfg.pollutants(), cfg.output_sector_level_name());

    switch (cfg.model_output_format()) {
    case ModelOuputFormat::Brn: {
        const auto pollutantParametersPath = cfg.data_root() / "05_model_parameters" / "pollutant_parameters.xlsx";
        auto pollutantParams               = parse_pollutant_parameters_config(pollutantParametersPath, cfg.pollutants());
        return std::make_unique<VlopsOutputBuilder>(std::move(sectorParams), std::move(pollutantParams), cfg);
    }
    case ModelOuputFormat::Dat: {
        const auto countryMappingPath = cfg.data_root() / "05_model_parameters" / "chimere_mapping_country.xlsx";
        auto countryMapping           = parse_chimere_country_mapping(countryMappingPath, cfg.countries());
        return std::make_unique<ChimereOutputBuilder>(std::move(sectorParams), std::move(countryMapping), cfg);
    }
    }

    throw RuntimeError("No known output builder for the specified grid definition");
}
}
