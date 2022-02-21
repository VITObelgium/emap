#pragma once

#include "gdx/algo/sum.h"
#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"
#include "gdx/rasterarea.h"

#include "infra/geometadata.h"
#include "infra/log.h"

namespace emap {

class GridRasterBuilder
{
public:
    GridRasterBuilder() = default;
    GridRasterBuilder(const inf::GeoMetadata& extent)
    : _raster(extent, extent.nodata.value())
    {
    }

    void add_raster(const gdx::DenseRaster<double>& ras)
    {
        auto intersection = inf::metadata_intersection(_raster.metadata(), ras.metadata());
        if (intersection.rows == 0 || intersection.cols == 0) {
            return;
        }

        auto subGrid1 = gdx::sub_area(_raster, intersection);
        auto subGrid2 = gdx::sub_area(ras, intersection);
        std::transform(subGrid1.begin(), subGrid1.end(), subGrid2.begin(), subGrid1.begin(), [](double res, double toAdd) {
            if (std::isnan(toAdd)) {
                return res;
            }

            if (std::isnan(res)) {
                return toAdd;
            }

            return res + toAdd;
        });
    }

    double current_sum() const
    {
        return gdx::sum(_raster);
    }

    void write_to_disk(const fs::path& path)
    {
        gdx::write_raster(std::move(_raster), path);
    }

private:
    gdx::DenseRaster<double> _raster;
};

}
