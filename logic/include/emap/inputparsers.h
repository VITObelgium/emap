#pragma once

#include "infra/filesystem.h"

#include <vector>

namespace emap {

class Emissions;
class ScalingFactors;

Emissions parse_emissions(const fs::path& emissionsCsv);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv);

}