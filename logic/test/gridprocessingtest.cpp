#include "emap/gridprocessing.h"

#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"

#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Grid processing")
{
    SUBCASE("transform grid")
    {
        /*const auto inputPath = fs::u8path("C:/Users/vdboerd/OneDrive - VITO/Documents/E-map/E-MAP/input/spatial patterns/co_A_PublicPower.tif");
        auto result = transform_grid(gdx::read_dense_raster<double>(inputPath), GridDefinition::Vlops1km);
        gdx::write_raster(result, "c:/temp/trans.tif");*/
    }
}
}
