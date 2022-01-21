#pragma once

#include "emap/emissions.h"
#include "gdx/denseraster.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <vector>

namespace emap {

class RunConfiguration;
class ScalingFactors;
class SectorInventory;
class PollutantInventory;

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, const RunConfiguration& cfg);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const RunConfiguration& cfg);
SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv, const RunConfiguration& cfg);

struct SpatialPatternData
{
    date::year year;
    EmissionIdentifier id;
    gdx::DenseRaster<double> raster;
};

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const RunConfiguration& cfg);

}