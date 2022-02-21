#include "gridrasterbuilder.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

TEST_CASE("Raster builder")
{
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
    GeoMetadata resultMeta(10, 10, 10000, 15000, 100, nan);
    GeoMetadata subMeta(2, 3, 10300, 15200, 100, nan);

    gdx::DenseRaster<double> subArea(subMeta, std::vector<double>{{1.0, nan, 3.0,
                                                                   4.0, 5.0, nan}});

    GridRasterBuilder builder(resultMeta);
    builder.add_raster(subArea);
    CHECK(builder.current_sum() == 13);
    builder.add_raster(subArea);
    CHECK(builder.current_sum() == 26);
    builder.add_raster(gdx::DenseRaster<double>(resultMeta, 1.0));
    CHECK(builder.current_sum() == 126);
    builder.add_raster(gdx::DenseRaster<double>(subMeta, 1.0));
    CHECK(builder.current_sum() == 132);
}
}
