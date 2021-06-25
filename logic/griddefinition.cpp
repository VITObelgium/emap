#include "emap/griddefinition.h"

#include "infra/enumutils.h"

#include <cassert>

namespace emap {

static const char* s_wgs84 = R"wkt(
PROJCS["WGS 84 / Pseudo-Mercator",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"],
    AUTHORITY["EPSG","3857"]]
)wkt";

using namespace inf;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

static const std::array<GridData, enum_value(GridDefinition::Count)> s_gridData{{
    {GridDefinition::Beleuros, GeoMetadata(0, 0, 0.0, 0.0, {100.0, -100}, nan, "")},
    {GridDefinition::Chimere1, GeoMetadata(45, 110, -116764.223, 6266274.438, {11233.540664545453183, -18200.716633333348000}, nan, s_wgs84)},
}};

const GridData& grid_data(GridDefinition grid) noexcept
{
    assert(enum_value(grid) < s_gridData.size());
    return s_gridData[enum_value(grid)];
}

}
