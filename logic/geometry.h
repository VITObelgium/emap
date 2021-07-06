#pragma once

#include "infra/point.h"

#include <polyclipping/clipper.hpp>

namespace inf::gdal {
class Geometry;
}

namespace emap::geom {

using Path  = ClipperLib::Path;
using Paths = ClipperLib::Paths;

// high performance geometry tests using the polyclipping library
// much faster than the gdal geometry routines

Paths from_gdal(inf::gdal::Geometry& geom);

void add_point_to_path(Path& path, inf::Point<int64_t> point);
void add_point_to_path(Path& path, inf::Point<double> point);

bool intersects(const Path& subject, const Paths& clip);

Paths intersect(const Path& subject, const Paths& clip);
Paths intersect(const Paths& subject, const Paths& clip);

double area(const Paths& paths);

}
