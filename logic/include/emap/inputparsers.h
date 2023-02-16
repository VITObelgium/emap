#pragma once

#include "emap/emissioninventory.h"
#include "emap/emissions.h"
#include "emap/spatialpatterndata.h"
#include "gdx/denseraster.h"
#include "infra/filesystem.h"
#include "infra/range.h"

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

static inf::Range<date::year> AllYears = inf::Range<date::year>(date::year(0), date::year(9999));

inf::Range<date::year> parse_year_range(std::string_view yearRange);
std::optional<double> pmcoarse_from_pm25_pm10(std::optional<double> pm25, std::optional<double> pm10);

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, date::year requestYear, const RunConfiguration& cfg, RespectIgnoreList respectIgnores);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const RunConfiguration& cfg);
SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactors, const RunConfiguration& cfg);

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const EmissionSector& sector, const RunConfiguration& cfg);
gdx::DenseRaster<double> parse_spatial_pattern_ceip(const fs::path& spatialPatternPath, const EmissionIdentifier& id, const RunConfiguration& cfg);

}