#pragma once

#include "infra/geometadata.h"

#include <vector>

namespace emap {

enum class ModelGrid
{
    Vlops1km,
    Vlops250m,
    Chimere05deg,
    Chimere01deg,
    Chimere005degLarge,
    Chimere005degSmall,
    Chimere0025deg,
    ChimereEmep,
    ChimereCams,
    ChimereRio1,
    ChimereRio4,
    ChimereRio32,
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
    Chimere05deg,
    Chimere01deg,
    Chimere005degLarge,
    Chimere005degSmall,
    Chimere0025deg,
    ChimereEmep,
    ChimereCams,
    ChimereRio1,
    ChimereRio4,
    ChimereRio32,
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
