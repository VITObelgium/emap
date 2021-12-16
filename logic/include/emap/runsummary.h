#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

namespace emap {

class RunSummary
{
public:
    void use_spatial_pattern_for_id(EmissionIdentifier id, const fs::path& grid);
    void use_uniform_distribution_for_id(EmissionIdentifier id);

private:
    struct UsedSpatialPattern
    {
        enum class Type
        {
            Grid,
            UniformDistribution,
        };

        EmissionIdentifier emissionId;
        Type type;
        std::optional<date::year> year;
    };

    std::vector<UsedSpatialPattern> _spatialPatterns;
};

}
