#include "geometry.h"

#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/gdalgeometry.h"
#include "infra/log.h"
#include "infra/parallelstl.h"

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

static constexpr double s_scalingFactor = 1e6;

static ClipperLib::IntPoint scaled_point(inf::Point<double> point) noexcept
{
    return {truncate<ClipperLib::cInt>(point.x * s_scalingFactor), truncate<ClipperLib::cInt>(point.y * s_scalingFactor)};
}

static ClipperLib::Paths from_gdal_polygon(gdal::Polygon& poly)
{
    ClipperLib::Paths result;

    {
        // Exterior ring
        auto gdalRing = poly.exterior_ring();

        ClipperLib::Path exterior;
        exterior.reserve(gdalRing.point_count());
        for (auto& point : gdalRing) {
            exterior.push_back(scaled_point(point));
        }

        if (!gdalRing.is_clockwise()) {
            ClipperLib::ReversePath(exterior);
        }

        result.push_back(std::move(exterior));
    }

    // interior rings
    for (int i = 0; i < poly.interior_ring_count(); ++i) {
        ClipperLib::Path interior;

        auto gdalRing = poly.interior_ring(i);

        interior.reserve(gdalRing.point_count());
        for (auto& point : gdalRing) {
            interior.emplace_back(scaled_point(point));
        }

        if (gdalRing.is_clockwise()) {
            ClipperLib::ReversePath(interior);
        }

        if (!interior.empty()) {
            result.emplace_back(std::move(interior));
        }
    }

    return result;
}

static ClipperLib::Paths from_gdal_multi_polygon(gdal::MultiPolygon& geom)
{
    ClipperLib::Paths result;

    for (int i = 0; i < geom.size(); ++i) {
        auto poly = geom.polygon_at(i);
        append_to_container(result, from_gdal_polygon(poly));
    }

    return result;
}

std::unique_ptr<geos::geom::LinearRing> gdal_linear_ring_to_geos(const geos::geom::GeometryFactory& factory, gdal::LinearRing& ring)
{
    std::vector<geos::geom::Coordinate> coords;
    coords.reserve(ring.point_count());

    for (int i = 0; i < ring.point_count(); ++i) {
        const auto point = ring.point_at(i);
        coords.emplace_back(point.x, point.y);
    }

    return factory.createLinearRing(geos::geom::DefaultCoordinateSequenceFactory::instance()->create(std::move(coords)));
}

static geos::geom::Polygon::Ptr gdal_polygon_to_geos(const geos::geom::GeometryFactory& factory, gdal::Polygon& poly)
{
    std::unique_ptr<geos::geom::LinearRing> exteriorRing;
    std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;

    {
        // Exterior ring
        auto gdalRing = poly.exterior_ring();
        exteriorRing  = gdal_linear_ring_to_geos(factory, gdalRing);
    }

    // interior rings
    for (int i = 0; i < poly.interior_ring_count(); ++i) {
        auto gdalRing = poly.interior_ring(i);
        holes.push_back(gdal_linear_ring_to_geos(factory, gdalRing));
    }

    return factory.createPolygon(std::move(exteriorRing), std::move(holes));
}

static geos::geom::MultiPolygon::Ptr gdal_multi_polygon_to_geos(const geos::geom::GeometryFactory& factory, gdal::MultiPolygon& geom)
{
    std::vector<std::unique_ptr<geos::geom::Geometry>> geometries;

    for (int i = 0; i < geom.size(); ++i) {
        auto poly = geom.polygon_at(i);
        geometries.push_back(gdal_polygon_to_geos(factory, poly));
    }

    return factory.createMultiPolygon(std::move(geometries));
}

Paths from_gdal(gdal::Geometry& geom)
{
    if (geom.type() == gdal::Geometry::Type::Polygon) {
        auto poly = geom.as<gdal::Polygon>();
        return from_gdal_polygon(poly);
    } else if (geom.type() == gdal::Geometry::Type::MultiPolygon) {
        auto poly = geom.as<gdal::MultiPolygon>();
        return from_gdal_multi_polygon(poly);
    }

    throw RuntimeError("Geometry type not implemented");
}

geos::geom::MultiPolygon::Ptr gdal_to_geos(inf::gdal::Geometry& geom)
{
    auto factory = geos::geom::GeometryFactory::create();

    if (geom.type() == gdal::Geometry::Type::Polygon) {
        std::vector<std::unique_ptr<geos::geom::Geometry>> geometries;
        auto poly = geom.as<gdal::Polygon>();
        geometries.push_back(gdal_polygon_to_geos(*factory, poly));
        return factory->createMultiPolygon(std::move(geometries));
    } else if (geom.type() == gdal::Geometry::Type::MultiPolygon) {
        auto poly = geom.as<gdal::MultiPolygon>();
        return gdal_multi_polygon_to_geos(*factory, poly);
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

void add_point_to_path(Path& path, inf::Point<int64_t> point)
{
    path.emplace_back(point.x, point.y);
}

void add_point_to_path(Path& path, inf::Point<double> point)
{
    path.push_back(scaled_point(point));
}

static gdal::Envelope bounding_box(const Path& path)
{
    gdal::Envelope env;

    for (auto& point : path) {
        env.merge(double(point.X), double(point.Y));
    }

    return env;
}

//static Rect<int64_t> combine_bbox(Rect<int64_t> lhs, Rect<int64_t> rhs)
//{
//    Rect<int64_t> result;
//
//    result.topLeft.x     = std::min(lhs.topLeft.x, rhs.topLeft.x);
//    result.bottomRight.x = std::max(lhs.bottomRight.x, rhs.bottomRight.x);
//
//    result.topLeft.y     = std::max(lhs.topLeft.y, rhs.topLeft.y);
//    result.bottomRight.y = std::min(lhs.bottomRight.y, rhs.bottomRight.y);
//
//    return result;
//}

static gdal::Envelope bounding_box(const Paths& path)
{
    gdal::Envelope env;

    for (const auto& path : path) {
        env.merge(bounding_box(path));
    }

    return env;
}

//template <typename T>
//static Rect<int64_t> bounding_box_intersection(const Rect<T>& lhs, const Rect<T>& rhs)
//{
//    gdal::Envelope env1(lhs.topLeft, lhs.bottomRight);
//    gdal::Envelope env2(rhs.topLeft, rhs.bottomRight);
//
//    if (!env1.intersects(env2)) {
//        return {};
//    }
//
//    auto topLeft     = Point<T>(std::max(lhs.topLeft.x, rhs.topLeft.x), std::min(lhs.topLeft.y, rhs.topLeft.y));
//    auto bottomRight = Point<T>(std::min(lhs.bottomRight.x, rhs.bottomRight.x), std::max(lhs.bottomRight.y, rhs.bottomRight.y));
//    return {topLeft, bottomRight};
//}

bool intersects(const Path& poly1, const Paths& poly2)
{
    for (auto& poly : poly2) {
        Paths solution;
        ClipperLib::MinkowskiDiff(poly1, poly, solution);
        if (ClipperLib::PointInPolygon(ClipperLib::IntPoint(0, 0), poly)) {
            return true;
        }
    }

    return false;
}

Paths intersect(const Path& subject, const Paths& clip)
{
    Paths result;

    if (bounding_box(subject).intersects(bounding_box(clip))) {
        ClipperLib::Clipper clipper;
        clipper.AddPath(subject, ClipperLib::ptSubject, true);
        clipper.AddPaths(clip, ClipperLib::ptClip, true);

        if (!clipper.Execute(ClipperLib::ctIntersection, result)) {
            throw RuntimeError("Intersection failed");
        }
    }

    return result;
}

Paths intersect(const Paths& subject, const Paths& clip)
{
    Paths result;

    if (bounding_box(subject).intersects(bounding_box(clip))) {
        ClipperLib::Clipper clipper;
        clipper.AddPaths(subject, ClipperLib::ptSubject, true);
        clipper.AddPaths(clip, ClipperLib::ptClip, true);

        if (!clipper.Execute(ClipperLib::ctIntersection, result)) {
            throw RuntimeError("Intersection failed");
        }
    }

    return result;
}

double area(const Paths& paths)
{
    double totalArea = 0.0;

    for (const auto& path : paths) {
        totalArea += ClipperLib::Area(path) / (s_scalingFactor * s_scalingFactor);
    }

    return totalArea;
}

}
