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

    // normalize the raster so the sum is 1
    if (const auto sum = gdx::sum(result); sum != 0.0) {
        gdx::transform(result, result, [sum](const auto& val) {
            return val / sum;
        });
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
            auto intersectGeometry   = preparedGeom->getGeometry().intersection(cellGeom.get());
            const auto intersectArea = intersectGeometry->getArea();
            assert(intersectArea > 0);
            result.emplace_back(cell, intersectArea / cellArea);
        }
    }

    return result;
}

static std::vector<std::pair<Country::Id, std::vector<CellCoverageInfo>>> process_country_borders(std::vector<std::pair<Country::Id, std::vector<CellCoverageInfo>>>& cellCoverages, GeoMetadata extent)
{
    std::vector<std::pair<Country::Id, std::vector<CellCoverageInfo>>> result;
    result.reserve(cellCoverages.size());

    for (auto& [country, cells] : cellCoverages) {
        std::vector<CellCoverageInfo> modifiedCoverages;
        modifiedCoverages.reserve(cells.size());

        std::vector<IntersectionInfo> intersectInfo;

        for (auto& cell : cells) {
            auto modifiedCoverage = cell;

            if (cell.coverage < 1.0) {
                // country border, check if there are other countries in this cell
                double otherCountryCoverages = 0;

                for (const auto& [testCountry, testCells] : cellCoverages) {
                    if (testCountry == country || Country(testCountry).is_sea() != Country(country).is_sea()) {
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

        file::write_as_text(fmt::format("c:/temp/em/{}_cells.geojson", Country(country).code()), rects_to_geojson(intersectInfo, extent.cellSize));

        result.emplace_back(country, std::move(modifiedCoverages));
    }

    return result;
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

    result = process_country_borders(result, extent);

    return result;
}

static GnfrSector detect_gnfr_sector_from_filename(const fs::path& filePath)
{
    // format: co_A_PublicPower

    const auto filename = filePath.stem().u8string();
    if (const auto pos = filename.find_first_of('_'); pos != std::string::npos && (pos + 1) < filename.size()) {
        return gnfr_sector_from_string(filename.substr(pos + 1));
    }

    throw RuntimeError("Could not determine sector from filename: {}", filename);
}

void extract_countries_from_raster(const fs::path& rasterInput, GnfrSector gnfrSector, const fs::path& countriesVector, const std::string& countryIdField, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb)
{
    const auto ras       = read_raster_north_up(rasterInput);
    const auto coverages = create_country_coverages(ras.metadata(), countriesVector, countryIdField, progressCb);
    return extract_countries_from_raster(rasterInput, gnfrSector, coverages, outputDir, filenameFormat, progressCb);
}

void extract_countries_from_raster(const fs::path& rasterInput, GnfrSector gnfrSector, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb)
{
    const auto ras = read_raster_north_up(rasterInput);
    fs::create_directories(outputDir);

    GridProcessingProgress progress(countries.size(), progressCb);

    for (const auto& [countryId, coverages] : countries) {
        Country country(countryId);
        const auto countryOutputPath = outputDir / fs::u8path(fmt::format(filenameFormat, country.code(), EmissionSector(gnfrSector)));
        gdx::write_raster(cutout_country(ras, coverages, gnfrSector), countryOutputPath);

        progress.set_payload(countryId);
        progress.tick_throw_on_cancel();
    }
}
}
