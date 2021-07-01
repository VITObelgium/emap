#include "emap/gridprocessing.h"
#include "emap/country.h"
#include "geometry.h"

#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/log.h"
#include "infra/math.h"
#include "infra/parallelstl.h"
#include "infra/rect.h"

#include <gdx/algo/algorithm.h>
#include <gdx/denseraster.h>
#include <gdx/denserasterio.h>
#include <gdx/rasteriterator.h>

namespace emap {

using namespace inf;
using namespace std::string_literals;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid)
{
    const auto& resultMeta = grid_data(grid).meta;

    return gdx::resample_raster(ras, resultMeta, gdal::ResampleAlgorithm::Sum);
}

std::vector<Rect<double>> create_raster_cell_geometry(const gdx::DenseRaster<double>& ras)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    OGRLineString cellGeometry;
    cellGeometry.setNumPoints(5);

    const auto cellSize = ras.metadata().cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    std::vector<Rect<double>> cellRects;

    std::for_each(gdx::cell_begin(ras), gdx::cell_end(ras), [&](const Cell& cell) {
        cellRects.push_back(ras.metadata().bounding_box(cell));
    });

    return cellRects;
}

struct IntersectionInfo
{
    IntersectionInfo() = default;
    IntersectionInfo(Rect<double> r, double c, double o)
    : rect(r)
    , cellArea(c)
    , overlapArea(o)
    {
    }

    Rect<double> rect;
    double cellArea    = 0.0;
    double overlapArea = 0.0;
};

std::string rects_to_geojson(std::span<const IntersectionInfo> intersections, GeoMetadata::CellSize cellSize)
{
    std::stringstream geomStr;
    geomStr << R"json(
    {
        "type": "FeatureCollection",
        "features": [
    )json";

    int i = 0;
    for (const auto& isect : intersections) {
        geomStr << "{ \"type\" : \"Feature\", \"geometry\" : { \"type\": \"Polygon\", \"coordinates\": [[";

        geomStr << fmt::format("[{}, {}], [{}, {}], [{}, {}], [{}, {}], [{}, {}]",
                               isect.rect.topLeft.x, isect.rect.topLeft.y,
                               isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y,
                               isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y + cellSize.y,
                               isect.rect.topLeft.x, isect.rect.topLeft.y + cellSize.y,
                               isect.rect.topLeft.x, isect.rect.topLeft.y);

        geomStr << "]]},";
        geomStr << fmt::format(R"json("properties": {{ "overlap": {}, "cell": {}, "perc": {} }})json", isect.overlapArea, isect.cellArea, isect.overlapArea / isect.cellArea);
        geomStr << "}";
        if (i + 1 < intersections.size()) {
            geomStr << ",";
        }

        ++i;
    }

    geomStr << "]}";

    return geomStr.str();
}

static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, const gdal::Geometry& country, std::string_view name)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    const auto cellSize = ras.metadata().cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    std::vector<IntersectionInfo> intersectRects;

    const auto geo = metadata_to_geo_transform(ras.metadata());

    if (!country.is_valid()) {
        throw RuntimeError("Invalid country geometry: {}", name);
    }

    auto countryEnvelope = country.envelope();

    std::for_each(gdx::cell_begin(ras), gdx::cell_end(ras), [&](const Cell& cell) {
        const Rect<double> box = ras.metadata().bounding_box(cell);
        gdal::Envelope cellEnv(box.topLeft, box.bottomRight);

        if (!countryEnvelope.intersects(cellEnv)) {
            return;
        }

        auto* cellLine = OGRGeometryFactory::createGeometry(wkbLineString)->toLineString();
        cellLine->setNumPoints(5);
        cellLine->setPoint(0, box.topLeft.x, box.topLeft.y);
        cellLine->setPoint(1, box.bottomRight.x, box.topLeft.y);
        cellLine->setPoint(2, box.bottomRight.x, box.bottomRight.y);
        cellLine->setPoint(3, box.topLeft.x, box.bottomRight.y);
        cellLine->setPoint(4, box.topLeft.x, box.topLeft.y);

        gdal::Owner<gdal::Geometry> cellPoly(OGRGeometryFactory::forceToPolygon(cellLine));

        if (country.contains(cellPoly)) {
            // Cell is completely inside the country
            intersectRects.emplace_back(box, cellArea, cellArea);
            return;
        }

        if (country.intersects(cellPoly)) {
            // Cell is partially inside the country
            const auto intersectionArea = country.intersection(cellPoly).area();

            // TODO: check if there is intersection with other countries, if no intersection
            // it means the inverse of the intersection is completely in the sea. In that case the
            // full amount is taken

            if (intersectionArea > 0) {
                result[cell] = (ras)[cell] * (*intersectionArea / cellArea);
            }

            intersectRects.emplace_back(box, cellArea, intersectionArea.value_or(-1.0));
        }
    });

    file::write_as_text(fmt::format("c:/temp/cells_{}.geojson", name), rects_to_geojson(intersectRects, cellSize));

    return result;
}

static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, const geom::Paths& country, std::string_view name)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    const auto cellSize = ras.metadata().cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    std::vector<IntersectionInfo> intersectRects;

    const auto geo = metadata_to_geo_transform(ras.metadata());

    std::for_each(gdx::cell_begin(ras), gdx::cell_end(ras), [&](const Cell& cell) {
        const Rect<double> box = ras.metadata().bounding_box(cell);
        gdal::Envelope cellEnv(box.topLeft, box.bottomRight);

        geom::Path cellPath;
        geom::add_point_to_path(cellPath, box.topLeft);
        geom::add_point_to_path(cellPath, Point<double>(box.bottomRight.x, box.topLeft.y));
        geom::add_point_to_path(cellPath, box.bottomRight);
        geom::add_point_to_path(cellPath, Point<double>(box.topLeft.x, box.bottomRight.y));
        geom::add_point_to_path(cellPath, box.topLeft);

        if (auto intersectArea = geom::area(geom::intersect(cellPath, country)); intersectArea > 0) {
            intersectRects.emplace_back(box, cellArea, intersectArea);
        }
    });

    file::write_as_text(fmt::format("c:/temp/clip_cells_{}.geojson", name), rects_to_geojson(intersectRects, cellSize));

    return result;
}

gdx::DenseRaster<double> read_raster_north_up(const fs::path& rasterInput)
{
    auto ras = gdx::read_dense_raster<double>(rasterInput);

    if (!ras.metadata().is_north_up()) {
        ras = gdx::warp_raster(ras, ras.metadata().projected_epsg().value());
    }

    return ras;
}

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& rasterOutput)
{
    constexpr const auto numCountries = enum_value(Country::Id::Count);

    const auto ras = read_raster_north_up(rasterInput);
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), math::nan<double>()), math::nan<double>());

    auto memDriver   = gdal::RasterDriver::create(gdal::RasterType::Memory);
    auto countriesDs = gdal::VectorDataSet::open(countriesShape);

    std::unique_ptr<gdal::RasterDataSet> ds;

    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index("FID");
    int bandCount           = 1;

    std::vector<gdx::DenseRaster<double>> rasters;
    std::vector<std::pair<Country::Id, geom::Paths>> countryGeometries;
    countryGeometries.reserve(countriesLayer.feature_count());

    for (auto& feature : countriesLayer) {
        if (const auto country = Country::try_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value()) {
            auto geom = feature.geometry();
            countryGeometries.emplace_back(country->id(), geom::from_gdal(geom));
        }
    }

    std::for_each(std::execution::par, countryGeometries.begin(), countryGeometries.end(), [&ras](const auto& idGeomPair) {
        Log::info("Clip Geom Country {}", Country(idGeomPair.first).code());
        cutout_country(ras, idGeomPair.second, Country(idGeomPair.first).code());
    });

    for (auto& feature : countriesLayer) {
        try {
            const auto country = Country::from_string(feature.field_as<std::string_view>(colCountryId));
            Log::info("Country {}", country);
            rasters.push_back(cutout_country(ras, feature.geometry(), country.code()));

            if (!ds) {
                ds = std::make_unique<gdal::RasterDataSet>(memDriver.create_dataset<double>(rasters.front().rows(), rasters.front().cols(), 0));
                ds->write_geometadata(ras.metadata());
            }

            ds->add_band(rasters.back().data());
            ds->set_band_description(bandCount++, std::string(country.code()));

            if (bandCount > 1) {
                //break;
            }
        } catch (const std::exception&) {
            // Not interested in this country
        }
    }

    auto tiffDriver = gdal::RasterDriver::create(rasterOutput);
    tiffDriver.create_dataset_copy(*ds, rasterOutput, std::span<const std::string>({"COMPRESS=LZW"s, "TILED=YES"s, "NUM_THREADS=ALL_CPUS"s}));
}

}
