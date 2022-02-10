#pragma once

#include "infra/geometadata.h"

namespace emap {

enum class GridDefinition
{
    Beleuros,
    Chimere1,
    Vlops60km,
    Vlops5km,
    Vlops1km,
    Vlops250m,
    Rio4x4,
    Rio4x4Extended,
    Flanders1km,
    CAMS,
    ChimereEmep,
    EnumCount,
    Invalid,
};

struct GridData
{
    GridDefinition type = GridDefinition::Invalid;
    std::string name;
    inf::GeoMetadata meta;
};

const GridData& grid_data(GridDefinition grid) noexcept;

}
