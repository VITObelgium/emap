#include "emap/gridprocessing.h"
#include "emap/emissions.h"
#include "geometry.h"

#include "infra/algo.h"
#include "infra/cast.h"
#include "infra/chrono.h"
#include "infra/crs.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/geometadata.h"
#include "infra/log.h"
#include "infra/math.h"
#include "infra/progressinfo.h"
#include "infra/rect.h"

#include <cassert>
#include <mutex>

#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/task_scheduler_observer.h>

#include <gdx/algo/sum.h>
#include <gdx/denseraster.h>
#include <gdx/denserasterio.h>
#include <gdx/rasteriterator.h>

#include <geos/geom/Geometry.h>
#include <geos/geom/prep/PreparedGeometryFactory.h>

namespace emap {

using namespace inf;
using namespace std::string_literals;

constexpr CellCoverageInfo::CellCoverageInfo() noexcept = default;

CellCoverageInfo::CellCoverageInfo(Cell c, double cov) noexcept
: cell(c)
, coverage(cov)
{
}

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

static std::string rects_to_geojson(std::span<const IntersectionInfo> intersections, GeoMetadata::CellSize cellSize)
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

static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, std::span<const CellCoverageInfo> coverages)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    for (auto& [cell, coverage] : coverages) {
        if (!ras.metadata().is_on_map(cell) || ras.is_nodata(cell)) {
            continue;
        }

        result[cell] = ras[cell] * coverage;
    }

    // normalize the raster so the sum is 1
    normalize_raster(result);

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

void normalize_raster(gdx::DenseRaster<double>& ras) noexcept
{
    // normalize the raster so the sum is 1
    if (const auto sum = gdx::sum(ras); sum != 0.0) {
        gdx::transform(ras, ras, [sum](const auto& val) {
            return val / sum;
        });
    }
}

gdx::DenseRaster<double> spread_values_uniformly_over_cells(double valueToSpread, GridDefinition grid, std::span<const CellCoverageInfo> cellCoverages)
{
    const auto totalCoverage = std::accumulate(cellCoverages.begin(), cellCoverages.end(), 0.0, [](double current, const CellCoverageInfo& cov) {
        return current + cov.coverage;
    });

    auto raster = cutout_country(gdx::DenseRaster<double>(grid_data(grid).meta, 1.0), cellCoverages);
    raster *= (valueToSpread / totalCoverage);
    return raster;
}

static std::vector<CellCoverageInfo> create_cell_coverages(const GeoMetadata& extent, const geos::geom::Geometry& geom)
{
    std::vector<CellCoverageInfo> result;

    auto preparedGeom = geos::geom::prep::PreparedGeometryFactory::prepare(&geom);

    const auto cellSize = extent.cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    for (auto cell : gdx::RasterCells(extent.rows, extent.cols)) {
        const Rect<double> box = extent.bounding_box(cell);
        const auto cellGeom    = geom::create_polygon(box.topLeft, box.bottomRight);

        // Intersect it with the country
        if (preparedGeom->contains(cellGeom.get())) {
            result.emplace_back(cell, 1.0);
        } else if (preparedGeom->intersects(cellGeom.get())) {
            auto intersectGeometry   = preparedGeom->getGeometry().intersection(cellGeom.get());
            const auto intersectArea = intersectGeometry->getArea();
            assert(intersectArea > 0);
            result.emplace_back(cell, intersectArea / cellArea);
        }
    }

    return result;
}

static std::vector<CountryCellCoverage> process_country_borders(const std::vector<CountryCellCoverage>& cellCoverages, GeoMetadata extent)
{
    std::vector<CountryCellCoverage> result;
    result.reserve(cellCoverages.size());

    for (auto& [country, cells, extent] : cellCoverages) {
        std::vector<CellCoverageInfo> modifiedCoverages;
        modifiedCoverages.reserve(cells.size());

        std::vector<IntersectionInfo> intersectInfo;

        for (auto& cell : cells) {
            auto modifiedCoverage = cell;

            if (cell.coverage < 1.0) {
                // country border, check if there are other countries in this cell
                double otherCountryCoverages = 0;

                for (const auto& [testCountry, testCells, testExtent] : cellCoverages) {
                    if (testCountry == country || testCountry.is_sea() != country.is_sea()) {
                        continue;
                    }

                    // Locate the current cell in the coverage of the other country
                    auto cellIter = std::lower_bound(testCells.begin(), testCells.end(), cell.cell, [](const CellCoverageInfo& cov, Cell c) {
                        return cov.cell < c;
                    });

                    if (cellIter != testCells.end() && cellIter->cell == cell.cell) {
                        // the other country covers the cell
                        otherCountryCoverages += cellIter->coverage;
                    }
                }

                if (otherCountryCoverages == 0.0) {
                    // This is the only (sea or land) country in the cell, so we get all the emissions
                    modifiedCoverage.coverage = 1.0;
                } else {
                    modifiedCoverage.coverage = cell.coverage / (cell.coverage + otherCountryCoverages);
                }

                IntersectionInfo intersect;
                intersect.cell             = cell.cell;
                intersect.percentage       = modifiedCoverage.coverage;
                intersect.cellArea         = extent.cell_size_x() * extent.cell_size_x();
                intersect.overlapArea      = cell.coverage;
                intersect.rect.topLeft     = extent.convert_cell_ll_to_xy(cell.cell);
                intersect.rect.bottomRight = intersect.rect.topLeft;
                intersect.rect.topLeft.y -= extent.cell_size_y();
                intersect.rect.bottomRight.x += extent.cell_size_x();

                intersectInfo.push_back(intersect);
            }

            modifiedCoverages.push_back(modifiedCoverage);
        }

        //file::write_as_text(fmt::format("c:/temp/em/{}_cells.geojson", Country(country).iso_code()), rects_to_geojson(intersectInfo, extent.cellSize));

        CountryCellCoverage cov;
        cov.country = country;
        cov.cells   = std::move(modifiedCoverages);
        cov.extent  = extent;
        result.push_back(std::move(cov));
    }

    return result;
}

size_t known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);

    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index(countryIdField);

    const auto bbox = extent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    size_t count = 0;
    for (auto& feature : countriesLayer) {
        if (inv.try_country_from_string(feature.field_as<std::string_view>(colCountryId)).has_value() && feature.has_geometry()) {
            ++count;
        }
    }

    return count;
}

GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const GeoMetadata& gridExtent)
{
    GeoMetadata geometryExtent = gridExtent;

    auto envelope = geom.getEnvelope();
    assert(!envelope->isEmpty());
    assert(envelope->getNumPoints() == 5);
    assert(envelope->getGeometryTypeId() == geos::geom::GeometryTypeId::GEOS_POLYGON);
    assert(envelope->isRectangle());

    const auto* env = envelope->getEnvelopeInternal();

    Point<double> topLeft(env->getMinX(), env->getMaxY());
    Point<double> bottomRight(env->getMaxX(), env->getMinY());

    auto topLeftCell     = gridExtent.convert_xy_to_cell(topLeft.x, topLeft.y);
    auto bottomRightCell = gridExtent.convert_xy_to_cell(bottomRight.x, bottomRight.y);

    if (topLeftCell.c > 0) {
        geometryExtent.xll += topLeftCell.c * gridExtent.cell_size_x();
        geometryExtent.cols -= topLeftCell.c;
    }

    if (topLeftCell.r > 0) {
        geometryExtent.rows -= topLeftCell.r;
    }

    if (bottomRightCell.c + 1 < gridExtent.cols) {
        geometryExtent.cols -= (gridExtent.cols - (bottomRightCell.c + 1));
    }

    if (bottomRightCell.r + 1 < gridExtent.rows) {
        int rowOffset = gridExtent.rows - (bottomRightCell.r + 1);

        geometryExtent.yll += rowOffset * gridExtent.cell_size_y();
        geometryExtent.rows -= rowOffset;
    }

    return geometryExtent;
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb)
{
    std::vector<CountryCellCoverage> result;

    auto countriesDs = gdal::VectorDataSet::open(countriesVector);

    auto countriesLayer = countriesDs.layer(0);
    auto colCountryId   = countriesLayer.layer_definition().required_field_index(countryIdField);

    assert(!extent.projection.empty());
    if (!countriesLayer.projection().has_value()) {
        throw RuntimeError("Invalid boundaries vector: No projection information available");
    }

    if (extent.geographic_epsg() != countriesLayer.projection()->epsg_geog_cs()) {
        throw RuntimeError("Projection mismatch between boundaries vector and spatial pattern grid EPSG:{} <-> EPSG:{}", extent.geographic_epsg().value(), countriesLayer.projection()->epsg_geog_cs().value());
    }

    const auto bbox = extent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    std::vector<std::pair<Country, geos::geom::Geometry::Ptr>> geometries;

    for (auto& feature : countriesLayer) {
        if (const auto country = inv.try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
            // known country
            auto geom = feature.geometry();
            geometries.emplace_back(*country, geom::gdal_to_geos(geom));
        }
    }

    // sort on geometry complexity, so we always start processing the most complex geometries
    // this avoids processing the most complext geometry in the end on a single core
    std::sort(geometries.begin(), geometries.end(), [](const std::pair<Country, geos::geom::Geometry::Ptr>& lhs, const std::pair<Country, geos::geom::Geometry::Ptr>& rhs) {
        return lhs.second->getNumPoints() >= rhs.second->getNumPoints();
    });

    {
        Log::debug("Create cell coverages");
        chrono::DurationRecorder rec;

        std::mutex mut;
        GridProcessingProgress progress(geometries.size(), progressCb);
        //std::for_each(geometries.begin(), geometries.end(), [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
        tbb::parallel_for_each(geometries, [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
            //auto countryExtent = create_geometry_extent(*idGeom.second, extent);

            auto coverages = create_cell_coverages(extent, *idGeom.second);
#ifndef NDEBUG
            if (!coverages.empty()) {
                for (size_t i = 0; i < coverages.size() - 1; ++i) {
                    if (!(coverages[i].cell < coverages[i + 1].cell)) {
                        throw std::logic_error("coverages should be sorted");
                    }
                }
            }
#endif

            progress.set_payload(idGeom.first);
            progress.tick();

            CountryCellCoverage cov;
            cov.country = idGeom.first;
            cov.cells   = std::move(coverages);

            std::scoped_lock lock(mut);
            result.push_back(std::move(cov));
        });

        Log::debug("Create cell coverages took: {}", rec.elapsed_time_string());
    }

    // sort the result on country code to get reproducible results in the process country borders function
    // as the cell coverages (double) of the neigboring countries are added, different order causes minor floating point additions differences
    std::sort(result.begin(), result.end(), [](const CountryCellCoverage& lhs, const CountryCellCoverage& rhs) {
        return lhs.country.iso_code() < rhs.country.iso_code();
    });

    // Update the coverages on the country borders to get appropriate spreading of the emissions
    // e.g.: if one cell contains 25% water from ocean1 and 25% water from ocean2 and 50% land the coverage of
    // both oceans will be modified to 50% each, as they will both receive half of the emission for sea sectors
    result = process_country_borders(result, extent);

    return result;
}

//void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesVector, const std::string& countryIdField, const fs::path& outputDir, std::string_view filenameFormat, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb)
//{
//    const auto ras       = read_raster_north_up(rasterInput);
//    const auto coverages = create_country_coverages(ras.metadata(), countriesVector, countryIdField, inv, progressCb);
//    return extract_countries_from_raster(rasterInput, coverages, outputDir, filenameFormat, progressCb);
//}

//void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb)
//{
//    const auto ras = read_raster_north_up(rasterInput);
//
//    GridProcessingProgress progress(countries.size(), progressCb);
//
//    for (const auto& [countryId, coverages, extent] : countries) {
//        if (countryId == country::BEF) {
//            progress.tick_throw_on_cancel();
//            continue;
//        }
//
//        Country country(countryId);
//        const auto countryOutputPath = outputDir / fs::u8path(fmt::format(filenameFormat, country.iso_code()));
//
//        gdx::write_raster(cutout_country(ras, coverages), countryOutputPath);
//
//        progress.set_payload(countryId);
//        progress.tick_throw_on_cancel();
//    }
//}

gdx::DenseRaster<double> extract_country_from_raster(const gdx::DenseRaster<double>& rasterInput, std::span<const CellCoverageInfo> cellCoverages)
{
    return cutout_country(rasterInput, cellCoverages);
}

gdx::DenseRaster<double> extract_country_from_raster(const fs::path& rasterInput, std::span<const CellCoverageInfo> cellCoverages)
{
    return cutout_country(read_raster_north_up(rasterInput), cellCoverages);
}

// generator<std::pair<gdx::DenseRaster<double>, Country>> extract_countries_from_raster(const fs::path& rasterInput, GnfrSector gnfrSector, std::span<const CountryCellCoverage> countries)
//{
//     const auto ras = read_raster_north_up(rasterInput);
//
//     for (const auto& [countryId, coverages] : countries) {
//         if (countryId == country::BEF) {
//             continue;
//         }
//
//         co_yield {cutout_country(ras, coverages), Country(countryId)};
//     }
// }
}
