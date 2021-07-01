#include "emap/gridprocessing.h"

#include "gdx/algo/sum.h"
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
        const auto inputPath   = fs::u8path("C:/Users/vdboerd/OneDrive - VITO/Documents/E-map/E-MAP/input/spatial patterns/co_A_PublicPower.tif");
        const auto inputRaster = gdx::read_dense_raster<double>(inputPath);
        auto result            = transform_grid(inputRaster, GridDefinition::Vlops1km);
        gdx::write_raster(result, "c:/temp/trans.tif");
    }

    SUBCASE("extract countries")
    {
        const auto inputPath     = fs::u8path("C:/Users/vdboerd/OneDrive - VITO/Documents/E-map/E-MAP/input/spatial patterns/co_A_PublicPower.tif");
        const auto outputPath    = fs::u8path("C:/temp/countries.tif");
        const auto countriesPath = fs::u8path("C:/RMABuild/emap/data/eu.gpkg");

        extract_countries_from_raster(inputPath, countriesPath, outputPath);
    }
}

}