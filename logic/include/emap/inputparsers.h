#pragma once

#include "emap/emissions.h"
#include "gdx/denseraster.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <vector>

namespace emap {

class ScalingFactors;
class SectorInventory;
class PollutantInventory;

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv);
SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv);
SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv); // TODO: still using this?
SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv);
ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv);

struct SpatialPatternData
{
    date::year year;
    EmissionIdentifier id;
    gdx::DenseRaster<double> raster;
};

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv);
SectorInventory parse_sectors(const fs::path& sectorSpec);
PollutantInventory parse_pollutants(const fs::path& pollutantSpec);

}