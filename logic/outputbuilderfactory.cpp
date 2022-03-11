#include "emap/outputbuilderfactory.h"

#include "brnoutputbuilder.h"
#include "emap/runconfiguration.h"

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

std::unordered_map<std::string, BrnOutputBuilder::SectorParameterConfig> parse_sector_parameters_config(const fs::path& diffuseParametersPath, SectorLevel level, std::string_view outputSectorLevelName)
{
    std::unordered_map<std::string, BrnOutputBuilder::SectorParameterConfig> result;

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

        BrnOutputBuilder::SectorParameterConfig config;
        config.hc_MW = feature.field_as<double>(colHc);
        config.h_m   = feature.field_as<double>(colH);
        config.s_m   = feature.field_as<double>(colS);
        config.tb    = feature.field_as<double>(colTb);
        config.id    = feature.field_as<int32_t>(colId);

        result.emplace(feature.field_as<std::string_view>(colSector), config);
    }

    return result;
}

std::unordered_map<std::string, BrnOutputBuilder::PollutantParameterConfig> parse_pollutant_parameters_config(const fs::path& pollutantParametersPath, const PollutantInventory& pollutants)
{
    std::unordered_map<std::string, BrnOutputBuilder::PollutantParameterConfig> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(pollutantParametersPath);

    auto layer = ds.layer("sd");

    const auto colPollutant = layer.layer_definition().required_field_index("pollutant_code");
    const auto colSd        = layer.layer_definition().required_field_index("sd");

    for (const auto& feature : layer) {
        if (!feature.field_is_valid(0)) {
            continue; // skip empty lines
        }

        BrnOutputBuilder::PollutantParameterConfig config;
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
    if (cfg.model_grid() == ModelGrid::Vlops1km || cfg.model_grid() == ModelGrid::Vlops250m) {
        const auto sectorParametersPath    = cfg.data_root() / "05_model_parameters" / "sector_parameters.xlsx";
        const auto pollutantParametersPath = cfg.data_root() / "05_model_parameters" / "pollutant_parameters.xlsx";

        auto sectorParams    = parse_sector_parameters_config(sectorParametersPath, cfg.output_sector_level(), cfg.output_sector_level_name());
        auto pollutantParams = parse_pollutant_parameters_config(pollutantParametersPath, cfg.pollutants());

        return std::make_unique<BrnOutputBuilder>(std::move(sectorParams), std::move(pollutantParams), cfg);
    }

    throw RuntimeError("No known output builder for the specified grid definition");
}
}