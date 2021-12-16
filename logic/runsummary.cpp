#include "emap/runsummary.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>

namespace emap {

using namespace inf;

void RunSummary::use_spatial_pattern_for_id(EmissionIdentifier id, const fs::path& grid)
{
    UsedSpatialPattern pattern;
    pattern.emissionId = id;
    pattern.type       = UsedSpatialPattern::Type::Grid;
    //pattern.year =

    _spatialPatterns.push_back(pattern);
}

void RunSummary::use_uniform_distribution_for_id(EmissionIdentifier id)
{
    UsedSpatialPattern pattern;
    pattern.emissionId = id;
    pattern.type       = UsedSpatialPattern::Type::UniformDistribution;

    _spatialPatterns.push_back(pattern);
}

}
