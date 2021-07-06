#include "emap/gridprocessing.h"
#include "geometry.h"

#include "infra/cast.h"
#include "infra/chrono.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/log.h"
#include "infra/math.h"
#include "infra/parallelstl.h"
#include "infra/rect.h"

#include <sys/_types/_size_t.h>
#include <tbb/parallel_pipeline.h>
#include <tbb/task_arena.h>
#include <tbb/task_scheduler_observer.h>

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

    std::vector<Rect<double>> cellRects;
    std::for_each(gdx::cell_begin(ras), gdx::cell_end(ras), [&](const Cell& cell) {
        cellRects.push_back(ras.metadata().bounding_box(cell));
    });

    return cellRects;
}

struct IntersectionInfo
{
    IntersectionInfo() noexcept = default;
    IntersectionInfo(Cell cl, Rect<double> r, double c, double o, double p)
    : cell(cl)
    , rect(r)
    , cellArea(c)
    , overlapArea(o)
    , percentage(p)
    {
    }

    Cell cell;
    Rect<double> rect;
    double cellArea    = 0.0;
    double overlapArea = 0.0;
    double percentage  = 0.0;
};

std::string rects_to_geojson(std::span<const IntersectionInfo> intersections, GeoMetadata::CellSize cellSize)
{
    std::stringstream geomStr;
    geomStr << R"json(
    {
        "type": "FeatureCollection",
        "features": [
    )json";

    size_t i = 0;
    for (const auto& isect : intersections) {
        geomStr << "{ \"type\" : \"Feature\", \"geometry\" : { \"type\": \"Polygon\", \"coordinates\": [[";

        geomStr << fmt::format("[{}, {}], [{}, {}], [{}, {}], [{}, {}], [{}, {}]",
                               isect.rect.topLeft.x, isect.rect.topLeft.y,
                               isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y,
                               isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y + cellSize.y,
                               isect.rect.topLeft.x, isect.rect.topLeft.y + cellSize.y,
                               isect.rect.topLeft.x, isect.rect.topLeft.y);

        geomStr << "]]},";
        geomStr << fmt::format(R"json("properties": {{ "overlap": {}, "cell": {}, "perc": {} }})json", isect.overlapArea, isect.cellArea, isect.percentage);
        geomStr << "}";
        if (i + 1 < intersections.size()) {
            geomStr << ",";
        }

        ++i;
    }

    geomStr << "]}";

    return geomStr.str();
}

//static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, const gdal::Geometry& country, std::string_view name)
//{
//    constexpr auto nan = math::nan<double>();
//    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);
//
//    const auto cellSize = ras.metadata().cellSize;
//    const auto cellArea = std::abs(cellSize.x * cellSize.y);
//
//    std::vector<IntersectionInfo> intersectRects;
//
//    const auto geo = metadata_to_geo_transform(ras.metadata());
//
//    if (!country.is_valid()) {
//        throw RuntimeError("Invalid country geometry: {}", name);
//    }
//
//    auto countryEnvelope = country.envelope();
//
//    std::for_each(gdx::cell_begin(ras), gdx::cell_end(ras), [&](const Cell& cell) {
//        const Rect<double> box = ras.metadata().bounding_box(cell);
//        gdal::Envelope cellEnv(box.topLeft, box.bottomRight);
//
//        if (!countryEnvelope.intersects(cellEnv)) {
//            return;
//        }
//
//        auto* cellLine = OGRGeometryFactory::createGeometry(wkbLineString)->toLineString();
//        cellLine->setNumPoints(5);
//        cellLine->setPoint(0, box.topLeft.x, box.topLeft.y);
//        cellLine->setPoint(1, box.bottomRight.x, box.topLeft.y);
//        cellLine->setPoint(2, box.bottomRight.x, box.bottomRight.y);
//        cellLine->setPoint(3, box.topLeft.x, box.bottomRight.y);
//        cellLine->setPoint(4, box.topLeft.x, box.topLeft.y);
//
//        gdal::Owner<gdal::Geometry> cellPoly(OGRGeometryFactory::forceToPolygon(cellLine));
//
//        if (country.contains(cellPoly)) {
//            // Cell is completely inside the country
//            intersectRects.emplace_back(cell, box, cellArea, cellArea);
//            return;
//        }
//
//        if (country.intersects(cellPoly)) {
//            Log::debug("Intersection");
//            //// Cell is partially inside the country
//            //const auto intersectionArea = country.intersection(cellPoly).area();
//
//            //// TODO: check if there is intersection with other countries, if no intersection
//            //// it means the inverse of the intersection is completely in the sea. In that case the
//            //// full amount is taken
//
//            //if (intersectionArea > 0) {
//            //    result[cell] = (ras)[cell] * (*intersectionArea / cellArea);
//            //}
//
//            //intersectRects.emplace_back(cell, box, cellArea, intersectionArea.value_or(-1.0));
//        }
//    });
//
//    file::write_as_text(fmt::format("c:/temp/cells_{}.geojson", name), rects_to_geojson(intersectRects, cellSize));
//
//    return result;
//}

static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, Country country, const std::unordered_map<Country::Id, geom::Paths>& countryGeometries, ProgressInfo::Callback progressCb)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    const auto cellSize = ras.metadata().cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    std::vector<IntersectionInfo> intersectRects;
    std::vector<IntersectionInfo> partialIntersections;

    const auto& geometry = countryGeometries.at(country.id());

    using IntersectionInfoPtr = std::shared_ptr<IntersectionInfo>;
    using CellPtr             = std::shared_ptr<Cell>;

    std::mutex mut;

    std::atomic<int64_t> currentCellNr(0);
    int64_t totalCells = ras.size();

    auto iter = gdx::cell_begin(ras);
    tbb::filter<void, CellPtr> source(tbb::filter_mode::serial_out_of_order, [&](tbb::flow_control& fc) -> CellPtr {
        if (progressCb) {
            if (progressCb(ProgressInfo::Status(++currentCellNr, totalCells)) == ProgressStatusResult::Abort) {
                fc.stop();
                return nullptr;
            }
        }

        if (iter == gdx::cell_end(ras)) {
            fc.stop();
            return nullptr;
        }

        return std::make_shared<Cell>(*iter++);
    });

    tbb::filter<CellPtr, IntersectionInfoPtr> transform(tbb::filter_mode::parallel, [&](const CellPtr& cell) -> IntersectionInfoPtr {
        try {
            const Rect<double> box = ras.metadata().bounding_box(*cell);

            // Create the geometry of the current cell
            geom::Path cellPath;
            geom::add_point_to_path(cellPath, box.topLeft);
            geom::add_point_to_path(cellPath, Point<double>(box.bottomRight.x, box.topLeft.y));
            geom::add_point_to_path(cellPath, box.bottomRight);
            geom::add_point_to_path(cellPath, Point<double>(box.topLeft.x, box.bottomRight.y));
            geom::add_point_to_path(cellPath, box.topLeft);

            // Intersect it with the country
            double percentage = 1.0;

            if (auto intersectArea = geom::area(geom::intersect(cellPath, geometry)); intersectArea > 0) {
                if (intersectArea < cellArea && std::abs(1.0 - (intersectArea / cellArea)) > 0.00001) {
                    double neighborOverlaps = 0.0;

                    for (const auto& [countryId, countryGeometry] : countryGeometries) {
                        if (countryId == country.id()) {
                            // ignore the current country
                            continue;
                        }

                        neighborOverlaps += geom::area(geom::intersect(cellPath, countryGeometry));
                    }

                    if (neighborOverlaps > 0.0) {
                        percentage = intersectArea / (neighborOverlaps + intersectArea);
                    } else {
                        // No overlap with other countries, so the rest of the cell is sea
                        // in that case the full amount can be assigned to this cell
                    }

                    {
                        std::scoped_lock lock(mut);
                        partialIntersections.emplace_back(*cell, box, cellArea, intersectArea, cellArea / intersectArea);
                    }
                }

                return std::make_shared<IntersectionInfo>(*cell, box, cellArea, intersectArea, percentage);
            }
        } catch (const std::exception& e) {
            Log::error("Failed to compute intersection for cell {} ({})", *cell, e.what());
        }

        return nullptr;
    });

    tbb::filter<IntersectionInfoPtr, void> sink(tbb::filter_mode::serial_out_of_order, [&](const IntersectionInfoPtr& intersection) {
        if (intersection) {
            intersectRects.push_back(*intersection);
            if (intersection->overlapArea > 0) {
                result[intersection->cell] = (ras)[intersection->cell] * (intersection->percentage);
            }
        }
    });

    tbb::filter<void, void> chain = source & transform & sink;

    const int maxTokens = tbb::this_task_arena::max_concurrency();
    tbb::parallel_pipeline(maxTokens, chain);

    file::write_as_text(fmt::format("c:/temp/clip_cells_{}.geojson", country.code()), rects_to_geojson(intersectRects, cellSize));
    file::write_as_text(fmt::format("c:/temp/clip_cells_partial_{}.geojson", country.code()), rects_to_geojson(partialIntersections, cellSize));

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

static bool is_complex_country(Country::Id id)
{
    return false;

    static bool warned = false;
    if (!warned) {
        Log::warn("This should never be present production code");
        warned = true;
    }
    return id == Country::Id::NO ||
           id == Country::Id::FR ||
           id == Country::Id::RU ||
           id == Country::Id::SE ||
           id == Country::Id::IT ||
           id == Country::Id::GL;
}

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& outputDir, GridProcessingProgress::Callback progressCb)
{
    const auto ras = read_raster_north_up(rasterInput);

    auto countriesDs = gdal::VectorDataSet::open(countriesShape);

    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index("FID");

    std::unordered_map<Country::Id, geom::Paths> countryGeometries;

    const auto bbox = ras.metadata().bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    fs::create_directories(outputDir);

    for (auto& feature : countriesLayer) {
        if (const auto country = Country::try_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value()) {
            auto geom = feature.geometry();

            if (!is_complex_country(country->id())) {
                countryGeometries.emplace(country->id(), geom::from_gdal(geom));
            }
        }
    }

    chrono::DurationRecorder totalRec;

    std::atomic<int64_t> currentCell(0);
    int64_t totalCellCount = ras.ssize() * countryGeometries.size();

    for (auto& [countryId, geom] : countryGeometries) {
        Country country(countryId);
        Log::debug("Country {} ({})", country.full_name(), country.code());

        chrono::DurationRecorder rec;
        const auto countryOutputPath = outputDir / fs::u8path(fmt::format("{}_{}.tif", rasterInput.stem().u8string(), country.code()));

        gdx::write_raster(cutout_country(ras, country, countryGeometries, [&](const ProgressInfo::Status& status) {
                              if (progressCb) {
                                  return progressCb(GridProcessingProgress::Status(++currentCell, totalCellCount, GridProcessingProgressInfo(country.id(), status.current(), status.total())));
                              } else {
                                  return ProgressStatusResult::Continue;
                              }
                          }),
                          countryOutputPath);
        Log::debug("{} took: {}", country.full_name(), rec.elapsed_time_string());
    }

    Log::debug("Total duration: {}", totalRec.elapsed_time_string());
}

}
