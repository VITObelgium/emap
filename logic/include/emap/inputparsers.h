#pragma once

#include "infra/filesystem.h"
#include "emap/emissions.h"
#include "gdx/denseraster.h"

#include <vector>
#include <date/date.h>

namespace emap {

class ScalingFactors;

SingleEmissions parse_emissions(const fs::path& emissionsCsv);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv);
SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData);
//SingleEmissions parse_emissions_flanders(const fs::path& emissionsData);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year);

struct SpatialPatternData
{
	date::year year;
	EmissionIdentifier id;
	gdx::DenseRaster<double> raster;
};

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath);

}