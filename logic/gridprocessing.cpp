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

/* Cut the country out of the grid, using the cellcoverage info and normalize the result */
static gdx::DenseRaster<double> cutout_country_normalized(const gdx::DenseRaster<double>& ras, const CountryCellCoverage& countryCoverage)
{
    auto result = cutout_country(ras, countryCoverage);
    normalize_raster(result);
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

gdx::DenseRaster<double> spread_values_uniformly_over_cells(double valueToSpread, const CountryCellCoverage& countryCoverage)
{
    const auto totalCoverage = std::accumulate(countryCoverage.cells.begin(), countryCoverage.cells.end(), 0.0, [](double current, const CountryCellCoverage::CellInfo& cellInfo) {
        return current + cellInfo.coverage;
    });

    auto raster = cutout_country(gdx::DenseRaster<double>(countryCoverage.outputSubgridExtent, 1.0), countryCoverage);
    raster *= (valueToSpread / totalCoverage);
    return raster;
}

static std::vector<CountryCellCoverage::CellInfo> create_cell_coverages(const GeoMetadata& extent, int32_t countryXOffset, int32_t countryYOffset, const geos::geom::Geometry& geom)
{
    std::vector<CountryCellCoverage::CellInfo> result;

    auto preparedGeom = geos::geom::prep::PreparedGeometryFactory::prepare(&geom);

    const auto cellSize = extent.cellSize;
    const auto cellArea = std::abs(cellSize.x * cellSize.y);

    for (auto cell : gdx::RasterCells(extent.rows, extent.cols)) {
        const Rect<double> box = extent.bounding_box(cell);
        const auto cellGeom    = geom::create_polygon(box.topLeft, box.bottomRight);

        // Intersect it with the country
        if (preparedGeom->contains(cellGeom.get())) {
            result.emplace_back(cell, Cell(cell.r + countryYOffset, cell.c + countryXOffset), 1.0);
        } else if (preparedGeom->intersects(cellGeom.get())) {
            auto intersectGeometry   = preparedGeom->getGeometry().intersection(cellGeom.get());
            const auto intersectArea = intersectGeometry->getArea();
            if (intersectArea > 0) {
                result.emplace_back(cell, Cell(cell.r + countryYOffset, cell.c + countryXOffset), intersectArea / cellArea);
            }
        }
    }

    return result;
}

static std::vector<CountryCellCoverage> process_country_borders(const std::vector<CountryCellCoverage>& cellCoverages, GeoMetadata extent)
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

size_t known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);
    return known_countries_in_extent(inv, extent, countriesDs, countryIdField);
}

size_t known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, gdal::VectorDataSet& countriesDs, const std::string& countryIdField)
{
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

GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const GeoMetadata& gridExtent, int32_t& xOffset, int32_t& yOffset)
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
    geometryExtent.cols = (bottomRightCell.c - topLeftCell.c) + 1;
    geometryExtent.rows = (bottomRightCell.r - topLeftCell.r) + 1;

    xOffset = -topLeftCell.c;
    yOffset = -topLeftCell.r;

    return geometryExtent;
}

inf::GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent, const gdal::SpatialReference& sourceProjection, int32_t& xOffset, int32_t& yOffset)
{
    gdal::SpatialReference destProj(gridExtent.projection);

    if (sourceProjection.epsg_cs() != destProj.epsg_cs()) {
        auto outputGeometry = geom.clone();
        geom::CoordinateWarpFilter warpFilter(sourceProjection.export_to_wkt().c_str(), gridExtent.projection.c_str());
        outputGeometry->apply_rw(warpFilter);
        return create_geometry_extent(*outputGeometry, gridExtent, xOffset, yOffset);
    } else {
        return create_geometry_extent(geom, gridExtent, xOffset, yOffset);
    }
}

CountryCellCoverage create_country_coverage(const Country& country, const geos::geom::Geometry& geom, const gdal::SpatialReference& geometryProjection, const GeoMetadata& outputExtent)
{
    CountryCellCoverage cov;

    int32_t xOffset = 0, yOffset = 0;

    const geos::geom::Geometry* geometry = &geom;
    geos::geom::Geometry::Ptr warpedGeometry;

    if (geometryProjection.epsg_cs() != outputExtent.projected_epsg()) {
        // clone the country geometry and warp it to the output grid projection
        warpedGeometry = geom.clone();
        geom::CoordinateWarpFilter warpFilter(outputExtent.projection.c_str(), outputExtent.projection.c_str());
        warpedGeometry->apply_rw(warpFilter);
        geometry = warpedGeometry.get();
    }

    cov.country             = country;
    cov.outputSubgridExtent = create_geometry_extent(*geometry, outputExtent, geometryProjection, xOffset, yOffset);
    cov.cells               = create_cell_coverages(outputExtent, xOffset, yOffset, *geometry);

#ifndef NDEBUG
    if (!cov.cells.empty()) {
        for (size_t i = 0; i < cov.cells.size() - 1; ++i) {
            if (!(cov.cells[i].computeGridCell < cov.cells[i + 1].computeGridCell)) {
                throw std::logic_error("coverages should be sorted");
            }
        }

        for (const auto& cellInfo : cov.cells) {
            assert(outputExtent.is_on_map(cellInfo.computeGridCell));
            assert(cov.outputSubgridExtent.is_on_map(cellInfo.countryGridCell));
        }
    }
#endif

    return cov;
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent, const fs::path& countriesVector, const std::string& countryIdField, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);
    return create_country_coverages(outputExtent, countriesDs, countryIdField, inv, progressCb);
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent, gdal::VectorDataSet& countriesDs, const std::string& countryIdField, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb)
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

    for (auto& feature : countriesLayer) {
        if (const auto country = inv.try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
            // known country
            geometries.emplace_back(*country, geom::gdal_to_geos(feature.geometry()));
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
            auto cov = create_country_coverage(idGeom.first, *idGeom.second, gdal::SpatialReference(projection), outputExtent);
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
    result = process_country_borders(result, outputExtent);

    return result;
}

// void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesVector, const std::string& countryIdField, const fs::path& outputDir, std::string_view filenameFormat, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb)
//{
//     const auto ras       = read_raster_north_up(rasterInput);
//     const auto coverages = create_country_coverages(ras.metadata(), countriesVector, countryIdField, inv, progressCb);
//     return extract_countries_from_raster(rasterInput, coverages, outputDir, filenameFormat, progressCb);
// }

// void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb)
//{
//     const auto ras = read_raster_north_up(rasterInput);
//
//     GridProcessingProgress progress(countries.size(), progressCb);
//
//     for (const auto& [countryId, coverages, extent] : countries) {
//         if (countryId == country::BEF) {
//             progress.tick_throw_on_cancel();
//             continue;
//         }
//
//         Country country(countryId);
//         const auto countryOutputPath = outputDir / fs::u8path(fmt::format(filenameFormat, country.iso_code()));
//
//         gdx::write_raster(cutout_country(ras, coverages), countryOutputPath);
//
//         progress.set_payload(countryId);
//         progress.tick_throw_on_cancel();
//     }
// }

gdx::DenseRaster<double> extract_country_from_raster(const gdx::DenseRaster<double>& rasterInput, const CountryCellCoverage& countryCoverage)
{
    return cutout_country(rasterInput, countryCoverage);
}

gdx::DenseRaster<double> extract_country_from_raster(const fs::path& rasterInput, const CountryCellCoverage& countryCoverage)
{
    auto raster = gdx::read_dense_raster<double>(rasterInput);
    return cutout_country(gdx::resample_raster(raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average), countryCoverage);
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
