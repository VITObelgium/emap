#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include "spatialpatterninventory.h"

namespace emap {

class RunSummary
{
public:
    void add_spatial_pattern_source(const SpatialPatternSource& source);
    void add_point_source(const fs::path& pointSource);
    void add_totals_source(const fs::path& totalsSource);

    std::string spatial_pattern_usage_table() const;
    std::string emission_source_usage_table() const;

private:
    std::vector<SpatialPatternSource> _spatialPatterns;
    std::vector<fs::path> _pointSources;
    std::vector<fs::path> _totalsSources;
};

}
