#include "geometry.h"

#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/log.h"

#include "infra/gdalgeometry.h"

#include <geos/geom/Coordinate.h>
#include <geos/geom/DefaultCoordinateSequenceFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineString.h>
#include <geos/geom/Polygon.h>

#include <memory>
#include <vector>

namespace emap::geom {

using namespace inf;

std::unique_ptr<geos::geom::LinearRing> gdal_linear_ring_to_geos(const geos::geom::GeometryFactory& factory, gdal::LinearRingCRef ring)
{
    std::vector<geos::geom::Coordinate> coords;
    coords.reserve(ring.point_count());

    for (int i = 0; i < ring.point_count(); ++i) {
        const auto point = ring.point_at(i);
        coords.emplace_back(point.x, point.y);
    }

    return factory.createLinearRing(geos::geom::DefaultCoordinateSequenceFactory::instance()->create(std::move(coords)));
}

static geos::geom::Polygon::Ptr gdal_polygon_to_geos(const geos::geom::GeometryFactory& factory, gdal::PolygonCRef poly)
{
    std::unique_ptr<geos::geom::LinearRing> exteriorRing;
    std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;

    {
        // Exterior ring
        exteriorRing = gdal_linear_ring_to_geos(factory, poly.exterior_ring());
    }

    // interior rings
    for (int i = 0; i < poly.interior_ring_count(); ++i) {
        holes.push_back(gdal_linear_ring_to_geos(factory, poly.interior_ring(i)));
    }

    return factory.createPolygon(std::move(exteriorRing), std::move(holes));
}

static geos::geom::MultiPolygon::Ptr gdal_multi_polygon_to_geos(const geos::geom::GeometryFactory& factory, gdal::MultiPolygonCRef geom)
{
    std::vector<std::unique_ptr<geos::geom::Geometry>> geometries;

    for (int i = 0; i < geom.size(); ++i) {
        geometries.push_back(gdal_polygon_to_geos(factory, geom.polygon_at(i)));
    }

    return factory.createMultiPolygon(std::move(geometries));
}

geos::geom::MultiPolygon::Ptr gdal_to_geos(inf::gdal::GeometryCRef geom)
{
    auto factory = geos::geom::GeometryFactory::create();

    if (geom.type() == gdal::Geometry::Type::Polygon) {
        std::vector<std::unique_ptr<geos::geom::Geometry>> geometries;
        geometries.push_back(gdal_polygon_to_geos(*factory, geom.as<gdal::PolygonCRef>()));
        return factory->createMultiPolygon(std::move(geometries));
    } else if (geom.type() == gdal::Geometry::Type::MultiPolygon) {
        return gdal_multi_polygon_to_geos(*factory, geom.as<gdal::MultiPolygonCRef>());
    }

    throw RuntimeError("Geometry type not implemented");
}

static std::unique_ptr<geos::geom::LinearRing> create_linear_ring(const geos::geom::GeometryFactory& factory, inf::Point<double> p1, inf::Point<double> p2)
{
    return factory.createLinearRing(geos::geom::DefaultCoordinateSequenceFactory::instance()->create({
        {p1.x, p1.y},
        {p2.x, p1.y},
        {p2.x, p2.y},
        {p1.x, p2.y},
        {p1.x, p1.y},
    }));
}

geos::geom::Polygon::Ptr create_polygon(inf::Point<double> p1, inf::Point<double> p2)
{
    const auto factory = geos::geom::GeometryFactory::getDefaultInstance();
    return factory->createPolygon(create_linear_ring(*factory, p1, p2));
}

geos::geom::LinearRing::Ptr create_linear_ring_from_rect(inf::Point<double> p1, inf::Point<double> p2)
{
    const auto factory = geos::geom::GeometryFactory::getDefaultInstance();
    return create_linear_ring(*factory, p1, p2);
}

void calculate_geometry_envelopes(const geos::geom::Geometry& geom)
{
    const auto numGeometries = geom.getNumGeometries();

    if (numGeometries == 1) {
        geom.getEnvelope();
    } else {
        for (size_t i = 0; i < numGeometries; ++i) {
            geom.getGeometryN(i)->getEnvelope();
        }
    }
}

}
