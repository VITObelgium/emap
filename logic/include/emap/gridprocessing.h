#pragma once

#include "emap/griddefinition.h"

#include <gdx/denseraster.h>
#include <gdx/denserasterio.h>

namespace emap {

using namespace inf;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid)
{
    const auto& resultMeta = grid_data(grid).meta;

    return gdx::resample_raster(ras, resultMeta, gdal::ResampleAlgorithm::Average);
}

}
