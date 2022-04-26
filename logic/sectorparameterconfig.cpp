#include "emap/sectorparameterconfig.h"

#include "infra/algo.h"

#include <map>

namespace emap {

using namespace inf;

Pollutant SectorParameterConfiguration::AnyPollutant("*", "Any pollutant");

void SectorParameterConfiguration::add_parameter(const std::string& sector, const SectorParameters& params)
{
    add_pollutant_specific_parameter(sector, AnyPollutant, params);
}

void SectorParameterConfiguration::add_pollutant_specific_parameter(const std::string& sector, const Pollutant& pollutant, const SectorParameters& params)
{
    PollutantSectorParameters polParams;
    polParams.pollutant = pollutant;
    polParams.hc_MW     = params.hc_MW;
    polParams.h_m       = params.h_m;
    polParams.s_m       = params.s_m;
    polParams.tb        = params.tb;
    polParams.id        = params.id;

    _parameters[sector].push_back(polParams);
}

SectorParameters SectorParameterConfiguration::get_parameters(const std::string& sector, const Pollutant& pollutant) const
{
    const auto& sectorParams = find_in_map_required(_parameters, sector);

    // Try to find an exact match
    auto pollutantParameters = find_in_container_optional(sectorParams, [&pollutant](const PollutantSectorParameters& params) {
        return params.pollutant == pollutant;
    });

    if (!pollutantParameters.has_value()) {
        pollutantParameters = find_in_container_optional(sectorParams, [](const PollutantSectorParameters& params) {
            return params.pollutant == AnyPollutant;
        });
    }

    if (!pollutantParameters.has_value()) {
        throw RuntimeError("No parameters configured for sector: {} and pollutant {}", sector, pollutant);
    }

    return SectorParameters(*pollutantParameters);
}

std::vector<std::string> SectorParameterConfiguration::sector_names_sorted_by_id() const
{
    std::map<int32_t, std::string> result;

    for (auto& [sectorName, params] : _parameters) {
        result.emplace(params.front().id, sectorName);
    }

    return map_values_as_vector(result);
}

}