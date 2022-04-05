#include "emap/debugtools.h"
#include "emap/configurationparser.h"
#include "emap/countryborders.h"
#include "emap/gridprocessing.h"
#include "geometry.h"

#include "infra/chrono.h"
#include "infra/gdal.h"
#include "infra/gdalalgo.h"

#include <geos/geom/DefaultCoordinateSequenceFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/MultiPolygon.h>

#include <oneapi/tbb/parallel_for_each.h>

namespace emap {

using namespace inf;

class VectorBuilder
{
public:
    VectorBuilder()
    : VectorBuilder("geom")
    {
    }

    VectorBuilder(const std::string& layerName)
    {
        auto memDriver = gdal::VectorDriver::create(gdal::VectorType::Memory);
        _ds            = memDriver.create_dataset();
        _ds.create_layer(layerName);
    }

    void set_projection(const std::string& projection)
    {
        gdal::SpatialReference srs(projection);
        set_projection(srs);
    }

    void set_projection(gdal::SpatialReference& srs)
    {
        _ds.layer(0).set_projection(srs);
    }

    template <typename T>
    int add_field(const std::string& name)
    {
        gdal::FieldDefinition fld(name, typeid(T));
        _ds.layer(0).create_field(fld);
        return _ds.layer(0).required_field_index(name);
    }

    void add_cell_geometry(int32_t r, int32_t c, gdal::GeometryCRef geom)
    {
        auto layer = _ds.layer(0);

        gdal::Feature feature(layer.layer_definition());
        feature.set_field("row", r);
        feature.set_field("col", c);
        feature.set_geometry(geom);
        layer.create_feature(feature);
    }

    void add_cell_geometry_with_coverage(int32_t r, int32_t c, double coverage, gdal::GeometryCRef geom)
    {
        auto layer = _ds.layer(0);

        gdal::Feature feature(layer.layer_definition());
        feature.set_field("row", r);
        feature.set_field("col", c);
        feature.set_field("coverage", coverage);
        feature.set_geometry(geom);
        layer.create_feature(feature);
    }

    void add_country_geometry(const Country& country, gdal::GeometryCRef geom)
    {
        auto layer = _ds.layer(0);

        gdal::Feature feature(layer.layer_definition());
        feature.set_field("country", std::string(country.iso_code()));
        feature.set_geometry(geom);
        layer.create_feature(feature);
    }

    void store(const fs::path& path) const
    {
        gdal::translate_vector_to_disk(_ds, path);
    }

private:
    gdal::VectorDataSet _ds;
};

void store_grid(const std::string& name, const inf::GeoMetadata& meta, const fs::path& path)
{
    auto memDriver = gdal::VectorDriver::create(gdal::VectorType::Memory);
    auto memDs     = memDriver.create_dataset();
    auto layer     = memDs.create_layer(name);

    gdal::SpatialReference srs(meta.projection);
    layer.set_projection(srs);

    gdal::FieldDefinition row("row", typeid(int32_t));
    gdal::FieldDefinition col("col", typeid(int32_t));
    layer.create_field(row);
    layer.create_field(col);

    auto rowIndex = layer.field_index(row.name());
    auto colIndex = layer.field_index(col.name());

    for (int r = 0; r < meta.rows; ++r) {
        for (int c = 0; c < meta.cols; ++c) {
            gdal::Feature feature(layer.layer_definition());
            feature.set_field(rowIndex, r);
            feature.set_field(colIndex, c);

            auto rect = meta.bounding_box(Cell(r, c));

            OGRLineString line;
            line.addPoint(rect.top_left().x, rect.top_left().y);
            line.addPoint(rect.top_right().x, rect.top_right().y);
            line.addPoint(rect.bottom_right().x, rect.bottom_right().y);
            line.addPoint(rect.bottom_left().x, rect.bottom_left().y);
            line.addPoint(rect.top_left().x, rect.top_left().y);

            gdal::LineCRef lineRef(&line);
            feature.set_geometry(lineRef);

            layer.create_feature(feature);
        }
    }

    gdal::translate_vector_to_disk(memDs, path);
}

void store_country_coverage_vector(const CountryCellCoverage& coverageInfo, const fs::path& path)
{
    VectorBuilder builder(fmt::format("{} cell coverages", coverageInfo.country.iso_code()));

    builder.set_projection(coverageInfo.outputSubgridExtent.projection);
    builder.add_field<int32_t>("row");
    builder.add_field<int32_t>("col");
    builder.add_field<double>("coverage");

    const auto& meta = coverageInfo.outputSubgridExtent;

    auto geomFactory = geos::geom::GeometryFactory::create();

    std::vector<geos::geom::Polygon::Ptr> cellPolygons;
    cellPolygons.reserve(meta.rows * meta.cols);

    for (const auto& cell : coverageInfo.cells) {
        auto rect = meta.bounding_box(cell.countryGridCell);

        OGRLinearRing ring;
        ring.addPoint(rect.top_left().x, rect.top_left().y);
        ring.addPoint(rect.top_right().x, rect.top_right().y);
        ring.addPoint(rect.bottom_right().x, rect.bottom_right().y);
        ring.addPoint(rect.bottom_left().x, rect.bottom_left().y);
        ring.addPoint(rect.top_left().x, rect.top_left().y);
        ring.closeRings();

        OGRPolygon poly;
        poly.addRing(&ring);

        gdal::PolygonCRef polyRef(&poly);
        builder.add_cell_geometry_with_coverage(cell.countryGridCell.r, cell.countryGridCell.c, cell.coverage, polyRef);
    }

    builder.store(path);
}

struct CountryGeometries
{
    std::string projection;
    std::vector<std::pair<Country, geos::geom::Geometry::Ptr>> geometries;
};

static CountryGeometries create_country_geometries(const fs::path& inputPath, const std::string& fieldId, const CountryInventory& countries, const std::string& gridProjection)
{
    CountryGeometries result;

    auto countriesDs    = gdal::VectorDataSet::open(inputPath);
    auto countriesLayer = countriesDs.layer(0);
    if (!countriesLayer.projection().has_value()) {
        throw RuntimeError("Invalid boundaries vector: No projection information available");
    }

    gdal::SpatialReference gridSpatialReference(gridProjection);

    auto colCountryId = countriesLayer.layer_definition().required_field_index(fieldId);
    result.projection = gridSpatialReference.export_to_wkt();

    {
        std::unordered_map<Country, geos::geom::Geometry::Ptr> geometriesMap;

        auto proj = countriesLayer.projection();
        for (auto& feature : countriesLayer) {
            if (const auto country = countries.try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
                // known country
                Log::info("Country: {}", country->full_name());
                auto gdalGeom = feature.geometry().clone();
                gdalGeom.transform_to(gridSpatialReference);

                auto geom = geom::gdal_to_geos(gdalGeom);
                if (geometriesMap.count(*country) == 0) {
                    geometriesMap.emplace(*country, std::move(geom));
                } else {
                    geometriesMap.insert_or_assign(*country, geom->Union(geometriesMap.at(*country).get()));
                }
            }
        }

        for (auto& [country, geometry] : geometriesMap) {
            result.geometries.emplace_back(country, std::move(geometry));
        }
    }

    // sort on geometry complexity, so we always start processing the most complex geometries
    // this avoids processing the most complext geometry in the end on a single core
    std::sort(result.geometries.begin(), result.geometries.end(), [](const std::pair<Country, geos::geom::Geometry::Ptr>& lhs, const std::pair<Country, geos::geom::Geometry::Ptr>& rhs) {
        return lhs.second->getNumPoints() >= rhs.second->getNumPoints();
    });

    return result;
}

static void process_geometries(const RunConfiguration& runConfig, const fs::path& boundaries, const std::string& fieldId, const std::string& suffix, const fs::path& outputDir)
{
    const auto gridProjection = grid_data(grids_for_model_grid(runConfig.model_grid()).front()).meta.projection;

    Log::info("Create country geometries");
    const auto countries = create_country_geometries(boundaries, fieldId, runConfig.countries(), gridProjection);

    const auto grids = grids_for_model_grid((runConfig.model_grid()));
    for (auto iter = grids.begin(); iter != grids.end(); ++iter) {
        const bool coursestGrid = iter == grids.begin();
        auto outputGridData     = grid_data(*iter);
        Log::info("Processing grid level {}", outputGridData.name);

        chrono::DurationRecorder dur;

        store_grid(fmt::format("Output grid ({})", outputGridData.name), outputGridData.meta, outputDir / fmt::format("output_grid_{}{}.gpkg", outputGridData.name, suffix));

        // std::for_each(countries.geometries.begin(), countries.geometries.end(), [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
        tbb::parallel_for_each(countries.geometries, [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
            const auto& country = idGeom.first;
            auto& geometry      = *idGeom.second;

            gdal::SpatialReference projection(countries.projection);

            Log::info("Process country: {} ({})", country.full_name(), country.iso_code());

            const auto env    = geometry.getEnvelope();
            const auto extent = create_geometry_intersection_extent(geometry, outputGridData.meta, projection);
            if (extent.rows <= 0 || extent.cols <= 0) {
                Log::info("No intersection for country: {} ({})", country.full_name(), country.iso_code());
                return;
            }

            auto coverageInfo = create_country_coverage(country, geometry, projection, outputGridData.meta, coursestGrid ? CoverageMode::AllCountryCells : CoverageMode::GridCellsOnly);
            if (!coverageInfo.cells.empty()) {
                store_country_coverage_vector(coverageInfo, outputDir / fmt::format("spatial_pattern_subgrid_{}_{}{}.gpkg", country.iso_code(), outputGridData.name, suffix));
            }
        });

        Log::info("Grid creation took {}", dur.elapsed_time_string());
    }
}

int debug_grids(const fs::path& runConfigPath, inf::Log::Level logLevel)
{
    std::unique_ptr<inf::LogRegistration> logReg;
    logReg = std::make_unique<inf::LogRegistration>("e-map");
    inf::Log::set_level(logLevel);

    try {
        auto runConfig       = parse_run_configuration_file(runConfigPath);
        const auto outputDir = runConfig.output_path() / "grids";

        store_grid("Spatial pattern grid CAMS", grid_data(GridDefinition::CAMS).meta, outputDir / "spatial_pattern_grid_cams.gpkg");
        store_grid("Spatial pattern grid CEIP", grid_data(GridDefinition::ChimereEmep).meta, outputDir / "spatial_pattern_grid_ceip.gpkg");
        store_grid("Spatial pattern grid Flanders", grid_data(GridDefinition::Flanders1km).meta, outputDir / "spatial_pattern_grid_flanders.gpkg");

        process_geometries(runConfig, runConfig.boundaries_vector_path(), runConfig.boundaries_field_id(), "", outputDir);
        process_geometries(runConfig, runConfig.eez_boundaries_vector_path(), runConfig.eez_boundaries_field_id(), "_eez", outputDir);

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        Log::error(e.what());
        fmt::print("{}\n", e.what());
        return EXIT_FAILURE;
    }
}
}
