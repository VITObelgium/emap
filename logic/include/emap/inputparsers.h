#pragma once

#include "emap/emissioninventory.h"
#include "emap/emissions.h"
#include "emap/spatialpatterndata.h"
#include "gdx/denseraster.h"
#include "infra/chrono.h"
#include "infra/filesystem.h"
#include "infra/range.h"
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

static inf::Range<inf::chrono::year> AllYears = inf::Range<inf::chrono::year>(inf::chrono::year(0), inf::chrono::year(9999));

inf::Range<inf::chrono::year> parse_year_range(std::string_view yearRange);

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, inf::chrono::year requestYear, const RunConfiguration& cfg, RespectIgnoreList respectIgnores);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, inf::chrono::year year, const RunConfiguration& cfg);
SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactors, const RunConfiguration& cfg);

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const EmissionSector& sector, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_ceip(const fs::path& spatialPatternPath, const EmissionIdentifier& id, const RunConfiguration& cfg);

}