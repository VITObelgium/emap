#pragma once

#include "emap/emissions.h"
#include "gdx/denseraster.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <vector>

namespace emap {

class ScalingFactors;

SingleEmissions parse_emissions(const fs::path& emissionsCsv);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year);
SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv);

struct SpatialPatternData
{
    date::year year;
    EmissionIdentifier id;
    gdx::DenseRaster<double> raster;
};

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath);

}