#include "emap/griddefinition.h"
#include "emap/inputparsers.h"
#include "emap/scalingfactors.h"

#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"
#include "infra/crs.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Grid definitions")
{
    SUBCASE("save a grid")
    {
        inf::GeoMetadata meta;
        meta.set_projection_from_epsg(crs::epsg::WGS84WebMercator);
        std::string projection = meta.projection;

        gdx::DenseRaster<float> ras(grid_data(GridDefinition::Chimere1).meta, 0.f);

        REQUIRE(ras.metadata().projected_epsg().has_value());
        CHECK(ras.metadata().projected_epsg().value() == crs::epsg::WGS84WebMercator);

        //gdx::write_raster(std::move(ras), "c:/temp/chimere1.tif");
    }
}
}
