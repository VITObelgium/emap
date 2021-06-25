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


static void write_raster(gdx::DenseRaster<float>&& ras, std::string_view filename)
{
    (void) ras;
    (void) filename;
    //gdx::write_raster(std::move(ras), fs::path("c:/temp") / fs::u8path(filename));
}

TEST_CASE("Grid definitions")
{
    SUBCASE("save a grid")
    {
        inf::GeoMetadata meta;
        meta.set_projection_from_epsg(crs::epsg::WGS84WebMercator);
        std::string projection = meta.projection;

        {
            gdx::DenseRaster<float> ras(grid_data(GridDefinition::Chimere1).meta, 0.f);
            REQUIRE(ras.metadata().projected_epsg().has_value());
            CHECK(ras.metadata().projected_epsg().value() == crs::epsg::WGS84WebMercator);
            write_raster(std::move(ras), "emap_chimere1.tif");
        }

        {
            gdx::DenseRaster<float> ras(grid_data(GridDefinition::Vlops1km).meta, 0.f);
            REQUIRE(ras.metadata().projected_epsg().has_value());
            CHECK(ras.metadata().projected_epsg().value() == crs::epsg::BelgianLambert72);
            write_raster(std::move(ras), "emap_vlops1km.tif");
        }

        {
            gdx::DenseRaster<float> ras(grid_data(GridDefinition::Vlops250m).meta, 0.f);
            REQUIRE(ras.metadata().projected_epsg().has_value());
            CHECK(ras.metadata().projected_epsg().value() == crs::epsg::BelgianLambert72);
            write_raster(std::move(ras), "emap_vlops250m.tif");
        }

        {
            gdx::DenseRaster<float> ras(grid_data(GridDefinition::Rio4x4).meta, 0.f);
            REQUIRE(ras.metadata().projected_epsg().has_value());
            CHECK(ras.metadata().projected_epsg().value() == crs::epsg::BelgianLambert72);
            write_raster(std::move(ras), "emap_rio4x4.tif");
        }

        {
            gdx::DenseRaster<float> ras(grid_data(GridDefinition::Rio4x4Extended).meta, 0.f);
            REQUIRE(ras.metadata().projected_epsg().has_value());
            CHECK(ras.metadata().projected_epsg().value() == crs::epsg::BelgianLambert72);
            write_raster(std::move(ras), "emap_rio4x4e.tif");
        }

    }
}
}
