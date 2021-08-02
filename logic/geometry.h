#pragma once

#include "infra/gdalgeometry.h"
#include "infra/point.h"

#include <geos/geom/GeometryComponentFilter.h>
#include <geos/geom/MultiPolygon.h>

namespace emap::geom {

// high performance geometry tests using the polyclipping library
// much faster than the gdal geometry routines

geos::geom::MultiPolygon::Ptr gdal_to_geos(inf::gdal::GeometryCRef geom);
geos::geom::Polygon::Ptr create_polygon(inf::Point<double> p1, inf::Point<double> p2);
geos::geom::LinearRing::Ptr create_linear_ring_from_rect(inf::Point<double> p1, inf::Point<double> p2);

void calculate_geometry_envelopes(const geos::geom::Geometry& geom);

}
