#pragma once

#include "infra/geometadata.h"

namespace emap {

enum class GridDefinition
{
    Beleuros,
    Chimere1,
    Vlops1km,
    Vlops250m,
    Rio4x4,
    Rio4x4Extended,
    Count,
};

struct GridData
{
    GridDefinition type = GridDefinition::Count;
    inf::GeoMetadata meta;
};

const GridData& grid_data(GridDefinition grid) noexcept;

}
