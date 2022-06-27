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

#include <gdx/algo/sum.h>
#include <gdx/denseraster.h>
#include <gdx/denserasterio.h>
#include <gdx/rasterarea.h>
#include <gdx/rasteriterator.h>

#include <geos/geom/Geometry.h>
#include <geos/geom/prep/PreparedGeometryFactory.h>

namespace emap {

using namespace inf;
using namespace std::string_literals;

gdal::VectorDataSet transform_vector(const fs::path& vectorPath, const GeoMetadata& destMeta)
{
    auto ds = gdal::VectorDataSet::open(vectorPath);

    // Clip the boundaries on the CAMS grid, we do not want to consider country geometries outside of the cams grid
    auto clipExtent = gdal::warp_metadata(destMeta, ds.layer(0).projection().value().export_to_wkt());

    // Only the features that intersect with the CAMS grid will be considered, avoids invalid geometry warnings near the poles
    auto [xMin, yMax] = clipExtent.top_left();
    auto [xMax, yMin] = clipExtent.bottom_right();

    // Clip everything within the output extent
    auto [xMinOut, yMaxOut] = destMeta.top_left();
    auto [xMaxOut, yMinOut] = destMeta.bottom_right();

    std::vector<std::string> options = {
        "-t_srs"s,
        destMeta.projection,
        "-spat",
        std::to_string(xMin),
        std::to_string(yMin),
        std::to_string(xMax),
        std::to_string(yMax),
        "-clipdst",
        std::to_string(xMinOut),
        std::to_string(yMinOut),
        std::to_string(xMaxOut),
        std::to_string(yMaxOut),
        "-nlt",
        "PROMOTE_TO_MULTI",
    };

    return gdal::translate_vector(ds, options);
}

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid, gdal::ResampleAlgorithm algo)
{
    const auto& resultMeta = grid_data(grid).meta;

    return gdx::resample_raster(ras, resultMeta, algo);
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

// static std::string rects_to_geojson(std::span<const IntersectionInfo> intersections, GeoMetadata::CellSize cellSize)
// {
//     std::stringstream geomStr;
//     geomStr << R"json(
//     {
//         "type": "FeatureCollection",
//         "features": [
//     )json";

//     size_t i = 0;
//     for (const auto& isect : intersections) {
//         geomStr << "{ \"type\" : \"Feature\", \"geometry\" : { \"type\": \"Polygon\", \"coordinates\": [[";

//         geomStr << fmt::format("[{}, {}], [{}, {}], [{}, {}], [{}, {}], [{}, {}]",
//                                isect.rect.topLeft.x, isect.rect.topLeft.y,
//                                isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y,
//                                isect.rect.topLeft.x + cellSize.x, isect.rect.topLeft.y + cellSize.y,
//                                isect.rect.topLeft.x, isect.rect.topLeft.y + cellSize.y,
//                                isect.rect.topLeft.x, isect.rect.topLeft.y);

//         geomStr << "]]},";
//         geomStr << fmt::format(R"json("properties": {{ "overlap": {}, "cell": {}, "perc": {} }})json", isect.overlapArea, isect.cellArea, isect.percentage);
//         geomStr << "}";
//         if (i + 1 < intersections.size()) {
//             geomStr << ",";
//         }

//         ++i;
//     }

//     geomStr << "]}";

//     return geomStr.str();
// }

/* Cut the country out of the grid, using the cellcoverage info, the extent of ras has to be the country subextent */
static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, const CountryCellCoverage& countryCoverage)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    for (const auto& cellInfo : countryCoverage.cells) {
        assert(ras.metadata().is_on_map(cellInfo.countryGridCell));
        if (!ras.metadata().is_on_map(cellInfo.countryGridCell)) {
            continue;
        }

        auto& cell = cellInfo.countryGridCell;
        if (ras.is_nodata(cellInfo.countryGridCell)) {
            continue;
        }

        result[cell] = ras[cell] * cellInfo.coverage;
    }

    return result;
}

gdx::DenseRaster<double> read_raster_north_up(const fs::path& rasterInput, const GeoMetadata& extent)
{
    auto ras = gdx::read_dense_raster<double>(rasterInput, extent);

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

void add_to_raster(gdx::DenseRaster<double>& collectedRaster, const gdx::DenseRaster<double>& countryRaster)
{
    auto intersection = inf::metadata_intersection(collectedRaster.metadata(), countryRaster.metadata());
    if (intersection.rows == 0 || intersection.cols == 0) {
        return;
    }

    auto subGrid1 = gdx::sub_area(collectedRaster, intersection);
    auto subGrid2 = gdx::sub_area(countryRaster, intersection);

    if (subGrid1.cols() != subGrid2.cols() || subGrid1.rows() != subGrid2.rows()) {
        throw RuntimeError("Country raster should be a subgrid of the grid raster");
    }

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

gdx::DenseRaster<double> spread_values_uniformly_over_cells(double valueToSpread, const CountryCellCoverage& countryCoverage)
{
    const auto totalCoverage = std::accumulate(countryCoverage.cells.begin(), countryCoverage.cells.end(), 0.0, [](double current, const CountryCellCoverage::CellInfo& cellInfo) {
        return current + cellInfo.coverage;
    });

    auto raster = cutout_country(gdx::DenseRaster<double>(countryCoverage.outputSubgridExtent, 1.0), countryCoverage);
    raster *= (valueToSpread / totalCoverage);
    return raster;
}

static std::vector<CountryCellCoverage::CellInfo> create_cell_coverages(const GeoMetadata& extent, const GeoMetadata& countryExtent, const geos::geom::Geometry& geom)
{
    std::vector<CountryCellCoverage::CellInfo> result;

    auto preparedGeom = geos::geom::prep::PreparedGeometryFactory::prepare(&geom);

    const auto cellSize = extent.cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    for (auto cell : gdx::RasterCells(countryExtent.rows, countryExtent.cols)) {
        const Rect<double> box = countryExtent.bounding_box(cell);
        const auto cellGeom    = geom::create_polygon(box.topLeft, box.bottomRight);

        // Intersect it with the country
        auto xyCentre   = countryExtent.convert_cell_centre_to_xy(cell);
        auto outputCell = extent.convert_point_to_cell(xyCentre);

        if (preparedGeom->contains(cellGeom.get())) {
            result.emplace_back(outputCell, cell, 1.0);
        } else if (preparedGeom->intersects(cellGeom.get())) {
            auto intersectGeometry   = preparedGeom->getGeometry().intersection(cellGeom.get());
            const auto intersectArea = intersectGeometry->getArea();
            if (intersectArea > 0) {
                result.emplace_back(outputCell, cell, intersectArea / cellArea);
            }
        }
    }

    return result;
}

static std::vector<CountryCellCoverage> process_country_borders(const std::vector<CountryCellCoverage>& cellCoverages)
{
    std::vector<CountryCellCoverage> result;
    result.reserve(cellCoverages.size());

    for (auto& [country, outputExtent, cells] : cellCoverages) {
        std::vector<CountryCellCoverage::CellInfo> modifiedCoverages;
        modifiedCoverages.reserve(cells.size());

        // std::vector<IntersectionInfo> intersectInfo;

        for (auto& cell : cells) {
            auto modifiedCoverage = cell;

            if (cell.coverage < 1.0) {
                // country border, check if there are other countries in this cell
                double otherCountryCoverages = 0;

                for (const auto& [testCountry, testOutputExtent, testCells] : cellCoverages) {
                    if (testCountry == country || testCountry.is_sea() != country.is_sea()) {
                        continue;
                    }

                    // Locate the current cell in the coverage of the other country
                    auto cellIter = std::lower_bound(testCells.begin(), testCells.end(), cell.computeGridCell, [](const CountryCellCoverage::CellInfo& cov, Cell c) {
                        return cov.computeGridCell < c;
                    });

                    if (cellIter != testCells.end() && cellIter->computeGridCell == cell.computeGridCell) {
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

                /*IntersectionInfo intersect;
                intersect.cell             = cell.computeGridCell;
                intersect.percentage       = modifiedCoverage.coverage;
                intersect.cellArea         = extent.cell_size_x() * extent.cell_size_x();
                intersect.overlapArea      = cell.coverage;
                intersect.rect.topLeft     = extent.convert_cell_ll_to_xy(cell.computeGridCell);
                intersect.rect.bottomRight = intersect.rect.topLeft;
                intersect.rect.topLeft.y -= extent.cell_size_y();
                intersect.rect.bottomRight.x += extent.cell_size_x();

                intersectInfo.push_back(intersect);*/
            }

            modifiedCoverages.push_back(modifiedCoverage);
        }

        // file::write_as_text(fmt::format("c:/temp/em/{}_cells.geojson", Country(country).iso_code()), rects_to_geojson(intersectInfo, extent.cellSize));

        CountryCellCoverage cov;
        cov.country             = country;
        cov.cells               = std::move(modifiedCoverages);
        cov.outputSubgridExtent = outputExtent;
        result.push_back(std::move(cov));
    }

    return result;
}

std::unordered_set<CountryId> known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);
    return known_countries_in_extent(inv, extent, countriesDs, countryIdField);
}

std::unordered_set<CountryId> known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, gdal::VectorDataSet& countriesDs, const std::string& countryIdField)
{
    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index(countryIdField);

    const auto bbox = extent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    std::unordered_set<CountryId> result;

    for (auto& feature : countriesLayer) {
        if (!feature.has_geometry()) {
            continue;
        }

        if (auto country = inv.try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value()) {
            result.insert(country->id());
        }
    }

    return result;
}

GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const GeoMetadata& gridExtent)
{
    GeoMetadata geometryExtent = gridExtent;

    const auto* env = geom.getEnvelopeInternal();

    Rect<double> geomRect;

    auto topLeft     = Point<double>(env->getMinX(), env->getMaxY());
    auto bottomRight = Point<double>(env->getMaxX(), env->getMinY());

    const auto topLeftCell     = gridExtent.convert_point_to_cell(topLeft);
    const auto bottomRightCell = gridExtent.convert_point_to_cell(bottomRight);

    const auto topLeftLL     = gridExtent.convert_cell_ll_to_xy(topLeftCell);
    const auto bottomRightLL = gridExtent.convert_cell_ll_to_xy(bottomRightCell);

    geomRect.topLeft     = Point<double>(topLeftLL.x, topLeftLL.y - gridExtent.cell_size_y());
    geomRect.bottomRight = Point<double>(bottomRightLL.x + gridExtent.cell_size_x(), bottomRightLL.y);

    geometryExtent.xll  = geomRect.topLeft.x;
    geometryExtent.yll  = geomRect.bottomRight.y;
    geometryExtent.cols = (bottomRightCell.c - topLeftCell.c) + 1;
    geometryExtent.rows = (bottomRightCell.r - topLeftCell.r) + 1;

    return geometryExtent;
}

inf::GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent, const gdal::SpatialReference& sourceProjection)
{
    gdal::SpatialReference destProj(gridExtent.projection);

    if (sourceProjection.epsg_cs() != destProj.epsg_cs()) {
        auto outputGeometry = geom.clone();
        geom::CoordinateWarpFilter warpFilter(sourceProjection.export_to_wkt().c_str(), gridExtent.projection.c_str());
        outputGeometry->apply_rw(warpFilter);
        return create_geometry_extent(*outputGeometry, gridExtent);
    } else {
        return create_geometry_extent(geom, gridExtent);
    }
}

GeoMetadata create_geometry_intersection_extent(const geos::geom::Geometry& geom, const GeoMetadata& gridExtent)
{
    GeoMetadata geometryExtent = gridExtent;

    const auto* env = geom.getEnvelopeInternal();

    Rect<double> geomRect;
    geomRect.topLeft     = Point<double>(env->getMinX(), env->getMaxY());
    geomRect.bottomRight = Point<double>(env->getMaxX(), env->getMinY());

    auto intersect = rectangle_intersection(geomRect, gridExtent.bounding_box());
    if (!intersect.is_valid() || intersect.width() == 0 || intersect.height() == 0) {
        // no intersection
        return {};
    }

    auto topLeftCell     = gridExtent.convert_point_to_cell(intersect.topLeft);
    auto bottomRightCell = gridExtent.convert_point_to_cell(intersect.bottomRight);

    auto lowerLeft = gridExtent.convert_cell_ll_to_xy(Cell(bottomRightCell.r, topLeftCell.c));

    geometryExtent.xll  = lowerLeft.x;
    geometryExtent.yll  = lowerLeft.y;
    geometryExtent.cols = std::max(0, (bottomRightCell.c - topLeftCell.c) + 1);
    geometryExtent.rows = std::max(0, (bottomRightCell.r - topLeftCell.r) + 1);

    return geometryExtent;
}

inf::GeoMetadata create_geometry_intersection_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent, const gdal::SpatialReference& sourceProjection)
{
    gdal::SpatialReference destProj(gridExtent.projection);

    if (sourceProjection.epsg_cs() != destProj.epsg_cs()) {
        auto outputGeometry = geom.clone();
        geom::CoordinateWarpFilter warpFilter(sourceProjection.export_to_wkt().c_str(), gridExtent.projection.c_str());
        outputGeometry->apply_rw(warpFilter);
        return create_geometry_intersection_extent(*outputGeometry, gridExtent);
    } else {
        return create_geometry_intersection_extent(geom, gridExtent);
    }
}

CountryCellCoverage create_country_coverage(const Country& country,
                                            const geos::geom::Geometry& geom,
                                            const gdal::SpatialReference& geometryProjection,
                                            const GeoMetadata& outputExtent,
                                            CoverageMode mode)
{
    CountryCellCoverage cov;

    const geos::geom::Geometry* geometry = &geom;
    geos::geom::Geometry::Ptr warpedGeometry;

    if (geometryProjection.epsg_cs() != outputExtent.projected_epsg()) {
        // clone the country geometry and warp it to the output grid projection
        warpedGeometry = geom.clone();
        geom::CoordinateWarpFilter warpFilter(outputExtent.projection.c_str(), outputExtent.projection.c_str());
        warpedGeometry->apply_rw(warpFilter);
        geometry = warpedGeometry.get();
    }

    cov.country = country;

    switch (mode) {
    case CoverageMode::GridCellsOnly:
        cov.outputSubgridExtent = create_geometry_intersection_extent(*geometry, outputExtent, geometryProjection);
        break;
    case CoverageMode::AllCountryCells:
        cov.outputSubgridExtent = create_geometry_extent(*geometry, outputExtent, geometryProjection);
        break;
    default:
        throw RuntimeError("Invalid coverage mode");
    }

    cov.cells = create_cell_coverages(outputExtent, cov.outputSubgridExtent, *geometry);

    return cov;
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent, const fs::path& countriesVector, const std::string& countryIdField, const CountryInventory& inv, CoverageMode mode, const GridProcessingProgress::Callback& progressCb)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);
    return create_country_coverages(outputExtent, countriesDs, countryIdField, inv, mode, progressCb);
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent,
                                                          gdal::VectorDataSet& countriesDs,
                                                          const std::string& countryIdField,
                                                          const CountryInventory& inv,
                                                          CoverageMode mode,
                                                          const GridProcessingProgress::Callback& progressCb)
{
    std::vector<CountryCellCoverage> result;

    auto countriesLayer = countriesDs.layer(0);
    auto colCountryId   = countriesLayer.layer_definition().required_field_index(countryIdField);

    assert(!outputExtent.projection.empty());
    if (!countriesLayer.projection().has_value()) {
        throw RuntimeError("Invalid boundaries vector: No projection information available");
    }

    if (outputExtent.geographic_epsg() != countriesLayer.projection()->epsg_geog_cs()) {
        throw RuntimeError("Projection mismatch between boundaries vector and spatial pattern grid EPSG:{} <-> EPSG:{}", outputExtent.geographic_epsg().value(), countriesLayer.projection()->epsg_geog_cs().value());
    }

    const auto bbox = outputExtent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    std::vector<std::pair<Country, geos::geom::Geometry::Ptr>> geometries;
    {
        std::unordered_map<Country, geos::geom::Geometry::Ptr> geometriesMap;

        for (auto& feature : countriesLayer) {
            if (const auto country = inv.try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
                // known country
                auto geom = geom::gdal_to_geos(feature.geometry());
                if (geometriesMap.count(*country) == 0) {
                    geometriesMap.emplace(*country, std::move(geom));
                } else {
                    geometriesMap.insert_or_assign(*country, geom->Union(geometriesMap.at(*country).get()));
                }
            }
        }

        for (auto& [country, geometry] : geometriesMap) {
            geometries.emplace_back(country, std::move(geometry));
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

        // export to string and import in every loop instance, accessing the spatial reference
        // from multiple threads is not thread safe
        auto projection = countriesLayer.projection().value().export_to_wkt();

        std::mutex mut;
        GridProcessingProgress progress(geometries.size(), progressCb);
        // std::for_each(geometries.rbegin(), geometries.rend(), [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
        tbb::parallel_for_each(geometries, [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
            auto cov = create_country_coverage(idGeom.first, *idGeom.second, gdal::SpatialReference(projection), outputExtent, mode);
            progress.set_payload(idGeom.first);
            progress.tick();

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
    result = process_country_borders(result);

    return result;
}

gdx::DenseRaster<double> extract_country_from_raster(const gdx::DenseRaster<double>& raster, const CountryCellCoverage& countryCoverage)
{
    return cutout_country(gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average), countryCoverage);
}

gdx::DenseRaster<double> extract_country_from_raster(const fs::path& rasterInput, const CountryCellCoverage& countryCoverage)
{
    return extract_country_from_raster(gdx::read_dense_raster<double>(rasterInput), countryCoverage);
}

void erase_area_in_raster(gdx::DenseRaster<double>& rasterInput, const inf::GeoMetadata& extent)
{
    auto rasterArea = gdx::sub_area(rasterInput, extent);
    std::fill(rasterArea.begin(), rasterArea.end(), std::numeric_limits<double>::quiet_NaN());
}

double erase_area_in_raster_and_sum_erased_values(gdx::DenseRaster<double>& rasterInput, const inf::GeoMetadata& extent)
{
    double sum = 0.0;

    auto rasterArea = gdx::sub_area_values(rasterInput, extent);
    std::for_each(rasterArea.begin(), rasterArea.end(), [&sum](auto& val) {
        sum += val;
        val = gdx::DenseRaster<double>::NaN;
    });

    return sum;
}

}
