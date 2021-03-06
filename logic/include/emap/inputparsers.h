#pragma once

#include "emap/emissioninventory.h"
#include "emap/emissions.h"
#include "emap/spatialpatterndata.h"
#include "gdx/denseraster.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <vector>

namespace emap {

class RunConfiguration;
class ScalingFactors;
class SectorInventory;
class PollutantInventory;

enum class RespectIgnoreList
{
    Yes,
    No,
};

std::optional<double> pmcoarse_from_pm25_pm10(std::optional<double> pm25, std::optional<double> pm10);

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, date::year requestYear, const RunConfiguration& cfg, RespectIgnoreList respectIgnores);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const RunConfiguration& cfg);
SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv, const RunConfiguration& cfg);

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const EmissionSector& sector, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_ceip(const fs::path& spatialPatternPath, const EmissionIdentifier& id, const RunConfiguration& cfg);

}