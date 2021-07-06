#include "geometry.h"

#include "infra/algo.h"
#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/gdalgeometry.h"
#include "infra/log.h"
#include "infra/parallelstl.h"

#include "infra/gdalgeometry.h"

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
