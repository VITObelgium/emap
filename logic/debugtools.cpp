#include "emap/debugtools.h"
#include "emap/configurationparser.h"
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
    double cellSizeX = meta.cell_size_x();
    double cellSizeY = meta.cell_size_y();

    auto topLeft = meta.top_left();

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
        double y = topLeft.y + (r * cellSizeY);

        for (int c = 0; c < meta.cols; ++c) {
            double x = topLeft.x + (c * cellSizeX);

            gdal::Feature feature(layer.layer_definition());
            feature.set_field(rowIndex, r);
            feature.set_field(colIndex, c);

            OGRLineString line;
            line.addPoint(x, y);
            line.addPoint(x + cellSizeX, y);
            line.addPoint(x + cellSizeX, y + cellSizeY);
            line.addPoint(x, y + cellSizeY);

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

    builder.set_projection(coverageInfo.spatialPatternSubgridExtent.projection);
    builder.add_field<int32_t>("row");
    builder.add_field<int32_t>("col");
    builder.add_field<double>("coverage");

    const auto& meta = coverageInfo.spatialPatternSubgridExtent;

    double cellSizeX = meta.cell_size_x();
    double cellSizeY = meta.cell_size_y();

    auto topLeft = meta.top_left();

    auto geomFactory = geos::geom::GeometryFactory::create();

    std::vector<geos::geom::Polygon::Ptr> cellPolygons;
    cellPolygons.reserve(meta.rows * meta.cols);

    for (const auto& cell : coverageInfo.cells) {
        auto ll = meta.convert_cell_ll_to_xy(cell.countryGridCell);

        OGRLinearRing ring;
        ring.addPoint(ll.x, ll.y - cellSizeY);
        ring.addPoint(ll.x + cellSizeX, ll.y - cellSizeY);
        ring.addPoint(ll.x + cellSizeX, ll.y);
        ring.addPoint(ll.x, ll.y);
        ring.closeRings();

        OGRPolygon poly;
        poly.addRing(&ring);

        gdal::PolygonCRef polyRef(&poly);
        builder.add_cell_geometry_with_coverage(cell.countryGridCell.r, cell.countryGridCell.c, cell.coverage, polyRef);
    }

    builder.store(path);
}

void debug_grids(const fs::path& runConfigPath, inf::Log::Level logLevel)
{
    std::unique_ptr<inf::LogRegistration> logReg;
    logReg = std::make_unique<inf::LogRegistration>("e-map");
    inf::Log::set_level(logLevel);

    auto runConfig = parse_run_configuration_file(runConfigPath);
    if (!runConfig.has_value()) {
        throw RuntimeError("No model configuration present");
    }

    const auto outputDir = runConfig->output_path() / "grids";

    auto countriesDs = gdal::VectorDataSet::open(runConfig->countries_vector_path());

    auto countriesLayer = countriesDs.layer(0);
    auto colCountryId   = countriesLayer.layer_definition().required_field_index(runConfig->country_field_id());

    if (!countriesLayer.projection().has_value()) {
        throw RuntimeError("Invalid boundaries vector: No projection information available");
    }

    auto outputGrid         = grid_data(runConfig->grid_definition());
    auto spatialPatternGrid = grid_data(GridDefinition::CAMS);

    std::vector<std::pair<Country, geos::geom::Geometry::Ptr>> geometries;

    {
        VectorBuilder countryGeometries("Country geometries");
        auto proj = countriesLayer.projection();
        countryGeometries.set_projection(proj.value());
        countryGeometries.add_field<std::string>("country");
        for (auto& feature : countriesLayer) {
            if (const auto country = runConfig->countries().try_country_from_string(feature.field_as<std::string_view>(colCountryId)); country.has_value() && feature.has_geometry()) {
                // known country
                countryGeometries.add_country_geometry(*country, feature.geometry());
                geometries.emplace_back(*country, geom::gdal_to_geos(feature.geometry()));
            }
        }

        countryGeometries.store(outputDir / "country_geometries.gpkg");
    }

    // sort on geometry complexity, so we always start processing the most complex geometries
    // this avoids processing the most complext geometry in the end on a single core
    std::sort(geometries.begin(), geometries.end(), [](const std::pair<Country, geos::geom::Geometry::Ptr>& lhs, const std::pair<Country, geos::geom::Geometry::Ptr>& rhs) {
        return lhs.second->getNumPoints() >= rhs.second->getNumPoints();
    });

    std::mutex mutex;

    auto projection = countriesLayer.projection().value().export_to_wkt();
    chrono::DurationRecorder dur;

    store_grid("Spatial pattern grid", grid_data(GridDefinition::CAMS).meta, outputDir / "spatial_pattern_grid.gpkg");
    store_grid(fmt::format("Output grid ({})", outputGrid.name), outputGrid.meta, outputDir / "output_grid.gpkg");

    tbb::parallel_for_each(geometries, [&](const std::pair<Country, geos::geom::Geometry::Ptr>& idGeom) {
        const auto& country = idGeom.first;
        auto& geometry      = *idGeom.second;

        int32_t xOffset, yOffset;

        const auto env = geometry.getEnvelope();
        auto extent    = create_geometry_extent(geometry, outputGrid.meta, gdal::SpatialReference(projection), xOffset, yOffset);
        if (extent.rows == 0 || extent.cols == 0) {
            Log::info("No intersection for country: {} ({})", country.full_name(), country.iso_code());
            return;
        }

        Log::info("Process country: {} ({})", country.full_name(), country.iso_code());
        auto coverageInfo = create_country_coverage(country, geometry, countriesLayer.projection().value(), spatialPatternGrid.meta, outputGrid.meta);
        store_country_coverage_vector(coverageInfo, outputDir / fmt::format("spatial_pattern_subgrid_{}.gpkg", country.iso_code()));
    });

    Log::info("Grid creation took {}", dur.elapsed_time_string());
}
}
