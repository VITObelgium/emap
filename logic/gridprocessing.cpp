#include "emap/gridprocessing.h"
#include "emap/emissions.h"
#include "geometry.h"

#include "infra/algo.h"
#include "infra/cast.h"
#include "infra/chrono.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/geometadata.h"
#include "infra/log.h"
#include "infra/math.h"
#include "infra/progressinfo.h"
#include "infra/rect.h"

#include <mutex>

#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/parallel_pipeline.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_scheduler_observer.h>

#include <gdx/denseraster.h>
#include <gdx/denserasterio.h>
#include <gdx/rasteriterator.h>

#include <geos/geom/Coordinate.h>
#include <geos/geom/DefaultCoordinateSequenceFactory.h>
#include <geos/geom/Envelope.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryComponentFilter.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/prep/PreparedGeometry.h>
#include <geos/geom/prep/PreparedGeometryFactory.h>
#include <geos/index/kdtree/KdTree.h>

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

static gdx::DenseRaster<double> cutout_country(const gdx::DenseRaster<double>& ras, std::span<const CellCoverageInfo> coverages, GnfrSector sector)
{
    constexpr auto nan = math::nan<double>();
    gdx::DenseRaster<double> result(copy_metadata_replace_nodata(ras.metadata(), nan), nan);

    EmissionSector emissionSector(sector);

    for (auto& [cell, coverage] : coverages) {
        if (ras.is_nodata(cell)) {
            continue;
        }

        if (emissionSector.is_land_sector()) {
            result[cell] = ras[cell] * coverage;
        } else {
            // TODO: sea sector logic
        }
    }

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
            const auto intersectArea = preparedGeom->getGeometry().intersection(cellGeom.get())->getArea();
            assert(intersectArea > 0);
            result.emplace_back(cell, intersectArea / cellArea);
        }
    }

    return result;
}

static void process_country_borders(std::vector<std::pair<Country::Id, std::vector<CellCoverageInfo>>>& cellCoverages)
{
    for (auto& [country, cells] : cellCoverages) {
        for (auto& cell : cells) {
            if (cell.coverage < 1.0) {
                // country border, check if there are other countries in this cell
                double otherCountryCoverages = 0;

                for (const auto& [testCountry, testCells] : cellCoverages) {
                    if (testCountry == country) {
                        continue;
                    }

                    auto cellIter = std::lower_bound(testCells.begin(), testCells.end(), cell.cell, [](const CellCoverageInfo& cov, Cell c) {
                        return cov.cell < c;
                    });

                    if (cellIter != testCells.end() && cellIter->cell == cell.cell) {
                        otherCountryCoverages += cellIter->coverage;
                    }
                }

                if (otherCountryCoverages == 0.0) {
                    // Cell partially covers the country, but no other countries are in this cell,
                    // so the rest must be ocean or sea. In that case the cell receives all the emission
                    cell.coverage = 1.0;
                } else {
                    cell.coverage = cell.coverage / (cell.coverage + otherCountryCoverages);
                }

                //cell.coverageSeaSector = 1.0 - (cell.coverageLandSector + otherCountryCoverages);
            }
        }
    }
}

size_t known_countries_in_extent(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField)
{
    auto countriesDs = gdal::VectorDataSet::open(countriesVector);

    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index(countryIdField);

    const auto bbox = extent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    size_t count = 0;
    for (auto& feature : countriesLayer) {
        if (Country::try_from_string(feature.field_as<std::string_view>(colCountryId)).has_value() && feature.has_geometry()) {
            ++count;
        }
    }

    return count;
}

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField, const GridProcessingProgress::Callback& progressCb)
{
    std::vector<CountryCellCoverage> result;

    auto countriesDs = gdal::VectorDataSet::open(countriesVector);

    auto countriesLayer     = countriesDs.layer(0);
    const auto colCountryId = countriesLayer.layer_definition().required_field_index(countryIdField);

    const auto bbox = extent.bounding_box();
    countriesLayer.set_spatial_filter(bbox.topLeft, bbox.bottomRight);

    std::vector<std::pair<Country::Id, geos::geom::Geometry::Ptr>> geometries;

    for (auto& feature : countriesLayer) {
        if (const auto country = Country::try_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
            // known country
            auto geom = feature.geometry();
            geometries.emplace_back(country->id(), geom::gdal_to_geos(geom));
        }
    }

    // sort on geometry complexity, so we always start processing the most complex geometries
    // this avoids processing the most complext geometry in the end on a single core
    std::sort(geometries.begin(), geometries.end(), [](const std::pair<Country::Id, geos::geom::Geometry::Ptr>& lhs, const std::pair<Country::Id, geos::geom::Geometry::Ptr>& rhs) {
        return lhs.second->getNumPoints() >= rhs.second->getNumPoints();
    });

    {
        Log::debug("Create cell coverages");
        chrono::DurationRecorder rec;

        std::mutex mut;
        GridProcessingProgress progress(geometries.size(), progressCb);
        tbb::parallel_for_each(geometries, [&](const std::pair<Country::Id, geos::geom::Geometry::Ptr>& idGeom) {
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
            std::scoped_lock lock(mut);
            result.emplace_back(idGeom.first, std::move(coverages));
        });

        Log::debug("Create cell coverages took: {}", rec.elapsed_time_string());
    }

    // sort the result on country id to get reproducible results in the process country borders function
    // as the cell coverages (double) of the neigboring countries are added, different order causes minor floating point additions differences
    std::sort(result.begin(), result.end(), [](const CountryCellCoverage& lhs, const CountryCellCoverage& rhs) {
        return lhs.first < rhs.first;
    });
    process_country_borders(result);

    return result;
}

static GnfrSector detect_gnfr_sector_from_filename(const fs::path& filePath)
{
    // co_A_PublicPower

    const auto filename = filePath.stem().u8string();
    if (const auto pos = filename.find_first_of('_'); pos != std::string::npos && (pos + 1) < filename.size()) {
        return gnfr_sector_from_string(filename.substr(pos + 1));
    }

    throw RuntimeError("Could not determine sector from filename: {}", filename);
}

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesVector, const std::string& countryIdField, const fs::path& outputDir, const GridProcessingProgress::Callback& progressCb)
{
    const auto ras       = read_raster_north_up(rasterInput);
    const auto coverages = create_country_coverages(ras.metadata(), countriesVector, countryIdField, progressCb);
    return extract_countries_from_raster(rasterInput, coverages, outputDir, progressCb);
}

void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, const GridProcessingProgress::Callback& progressCb)
{
    const auto ras        = read_raster_north_up(rasterInput);
    const auto gnfrSector = detect_gnfr_sector_from_filename(rasterInput);

    fs::create_directories(outputDir);

    GridProcessingProgress progress(countries.size(), progressCb);

    for (const auto& [countryId, coverages] : countries) {
        Country country(countryId);
        const auto countryOutputPath = outputDir / fs::u8path(fmt::format("{}_{}.tif", rasterInput.stem().u8string(), country.code()));
        gdx::write_raster(cutout_country(ras, coverages, gnfrSector), countryOutputPath);

        progress.set_payload(countryId);
        progress.tick_throw_on_cancel();
    }
}
}
