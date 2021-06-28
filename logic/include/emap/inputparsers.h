#pragma once

#include "infra/filesystem.h"
#include "emap/emissions.h"

#include <vector>

namespace emap {

class ScalingFactors;

SingleEmissions parse_emissions(const fs::path& emissionsCsv);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv);
SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData);
SingleEmissions parse_emissions_flanders(const fs::path& emissionsData);

}