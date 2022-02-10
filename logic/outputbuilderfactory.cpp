#include "emap/outputbuilderfactory.h"

#include "brnoutputbuilder.h"
#include "emap/runconfiguration.h"

#include "infra/gdal.h"
#include "infra/log.h"

#include <unordered_map>

namespace emap {

using namespace inf;

std::unordered_map<int32_t, BrnOutputBuilder::SectorParameterConfig> parse_sector_parameters_config(const fs::path& diffuseParametersPath, SectorLevel level, std::string_view outputSectorLevelName, const SectorInventory& sectors)
{
    std::unordered_map<int32_t, BrnOutputBuilder::SectorParameterConfig> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(diffuseParametersPath);

    if (level == SectorLevel::NFR) {
        auto layer = ds.layer("nfr");

        const auto colNfr = layer.layer_definition().required_field_index("NFR Code");
        const auto colHc  = layer.layer_definition().required_field_index("hc(MW)");
        const auto colH   = layer.layer_definition().required_field_index("h(m)");
        const auto colS   = layer.layer_definition().required_field_index("s(m)");
        const auto colTb  = layer.layer_definition().required_field_index("tb");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            BrnOutputBuilder::SectorParameterConfig config;
            config.hc_MW = feature.field_as<double>(colHc);
            config.h_m   = feature.field_as<double>(colH);
            config.s_m   = feature.field_as<double>(colS);
            config.tb    = feature.field_as<double>(colTb);

            if (
                auto sector = sectors.try_nfr_sector_from_string(feature.field_as<std::string_view>(colNfr)); sector.has_value()) {
                result.emplace(sector->id(), config);
            } else {
                Log::warn("Unknown sector name in parameter configuration: {}", feature.field_as<std::string_view>(colNfr));
            }
        }
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
    if (cfg.grid_definition() == GridDefinition::Vlops60km || cfg.grid_definition() == GridDefinition::Vlops1km || cfg.grid_definition() == GridDefinition::Vlops250m) {
        const auto sectorParametersPath    = cfg.data_root() / "05_model_parameters" / "parameters_diffuus.xlsx";
        const auto pollutantParametersPath = cfg.data_root() / "05_model_parameters" / "parameter_sd.xlsx";

        auto sectorParams    = parse_sector_parameters_config(sectorParametersPath, cfg.output_sector_level(), cfg.output_sector_level_name(), cfg.sectors());
        auto pollutantParams = parse_pollutant_parameters_config(pollutantParametersPath, cfg.pollutants());

        return std::make_unique<BrnOutputBuilder>(std::move(sectorParams), std::move(pollutantParams), truncate<int32_t>(grid_data(cfg.grid_definition()).meta.cell_size_x()));
    }

    throw RuntimeError("No known output builder for the specified grid definition");
}

}