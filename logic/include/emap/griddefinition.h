#pragma once

#include "infra/geometadata.h"

namespace emap {

enum class GridDefinition
{
    Beleuros,
    Chimere1,
    Count,
};

struct GridData
{
    GridDefinition type = GridDefinition::Count;
    inf::GeoMetadata meta;
};

const GridData& grid_data(GridDefinition grid) noexcept;

}
