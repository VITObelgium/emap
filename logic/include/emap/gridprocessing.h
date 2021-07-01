#pragma once

#include "emap/griddefinition.h"
#include "infra/filesystem.h"

#include <gdx/rasterfwd.h>

namespace emap {

using namespace inf;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid);

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& rasterOutput);

}
 