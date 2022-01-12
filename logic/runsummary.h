#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include "spatialpatterninventory.h"

namespace emap {

class RunSummary
{
public:
    void add_spatial_pattern_source(const SpatialPatternSource& source);

    std::string spatial_pattern_usage_table() const;

private:
    std::vector<SpatialPatternSource> _spatialPatterns;
};

}
