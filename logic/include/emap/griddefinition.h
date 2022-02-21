#pragma once

#include "infra/geometadata.h"

#include <vector>

namespace emap {

enum class ModelGrid
{
    Vlops1km,
    Vlops250m,
    EnumCount,
    Invalid,
};

enum class GridDefinition
{
    Beleuros,
    Chimere1,
    Vlops60km,
    Vlops5km,
    Vlops1km,
    Vlops250m,
    VlopsCalc,
    Rio4x4,
    Rio4x4Extended,
    Flanders1km,
    CAMS,
    ChimereEmep,
    EnumCount,
    Invalid,
};

std::vector<GridDefinition> grids_for_model_grid(ModelGrid grid);

struct GridData
{
    GridDefinition type = GridDefinition::Invalid;
    std::string name;
    inf::GeoMetadata meta;
};

const GridData& grid_data(GridDefinition grid) noexcept;

}
