#pragma once

#include "emap/country.h"
#include "emap/griddefinition.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

#include <gdx/rasterfwd.h>

namespace emap {

using namespace inf;

struct GridProcessingProgressInfo
{
    GridProcessingProgressInfo(Country::Id country_, int64_t currentCell_, int64_t cellCount_)
    : country(country_)
    , currentCell(currentCell_)
    , cellCount(cellCount_)
    {
    }

    Country::Id country = Country::Id::Invalid;
    int64_t currentCell = 0;
    int64_t cellCount   = 0;
};

using GridProcessingProgress = inf::ProgressTracker<GridProcessingProgressInfo>;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid);

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& outputDir, GridProcessingProgress::Callback progressCb = nullptr);


}
