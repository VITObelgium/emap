﻿#include "emap/griddefinition.h"

#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <cassert>
#include <type_traits>

namespace emap {

static const char* s_epsg3857 = R"wkt(PROJCS["WGS 84 / Pseudo-Mercator",
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

static const char* s_epsg4326 = R"wkt(GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Latitude",NORTH],
    AXIS["Longitude",EAST],
    AUTHORITY["EPSG","4326"]]
)wkt";

static const char* s_belgianLambert72 = R"wkt(PROJCS["Belge 1972 / Belgian Lambert 72",
    GEOGCS["Belge 1972",
        DATUM["Reseau_National_Belge_1972",
            SPHEROID["International 1924",6378388,297,
                AUTHORITY["EPSG","7022"]],
            AUTHORITY["EPSG","6313"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4313"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["latitude_of_origin",90],
    PARAMETER["central_meridian",4.36748666666667],
    PARAMETER["standard_parallel_1",51.1666672333333],
    PARAMETER["standard_parallel_2",49.8333339],
    PARAMETER["false_easting",150000.013],
    PARAMETER["false_northing",5400088.438],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    AUTHORITY["EPSG","31370"]]
)wkt";

static const char* s_epsg3035 = R"wkt(PROJCS["ETRS89-extended / LAEA Europe",
    GEOGCS["ETRS89",
        DATUM["European_Terrestrial_Reference_System_1989",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6258"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4258"]],
    PROJECTION["Lambert_Azimuthal_Equal_Area"],
    PARAMETER["latitude_of_center",52],
    PARAMETER["longitude_of_center",10],
    PARAMETER["false_easting",4321000],
    PARAMETER["false_northing",3210000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","3035"]]
)wkt";

using namespace inf;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

static const std::array<GridData, enum_count<GridDefinition>()> s_gridData{{
    {GridDefinition::Beleuros, "Beleuros", GeoMetadata(0, 0, 0.0, 0.0, {100.0, -100}, nan, "")},
    {GridDefinition::Chimere1, "Chimere 1", GeoMetadata(45, 110, -116764.223, 6266274.438, {11233.540664545453183, -18200.716633333348000}, nan, s_epsg3857)},
    {GridDefinition::Vlops60km, "Vlops 60km", GeoMetadata(56, 52, -1419000, -1480000, {60000.0, -60000.0}, nan, s_belgianLambert72)},
    {GridDefinition::Vlops5km, "Vlops 5km", GeoMetadata(120, 144, -219000, -100000, {5000.0, -5000.0}, nan, s_belgianLambert72)},
    {GridDefinition::Vlops1km, "Vlops 1km", GeoMetadata(120, 260, 11000.0, 140000.0, {1000.0, -1000.0}, nan, s_belgianLambert72)},
    {GridDefinition::Vlops250m, "Vlops 250m", GeoMetadata(480, 1040, 11000.0, 140000.0, {250.0, -250.0}, nan, s_belgianLambert72)},
    {GridDefinition::VlopsCalc, "Vlops 250m full area", GeoMetadata(13440, 12480, -1419000, -1480000, {250.0, -250.0}, nan, s_belgianLambert72)},
    {GridDefinition::Rio4x4, "RIO 4x4", GeoMetadata(57, 69, 22000.0, 20000.0, {4000.0, -4000.0}, nan, s_belgianLambert72)},
    {GridDefinition::Rio4x4Extended, "RIO 4x4 extended", GeoMetadata(61, 73, 14000.0, 12000.0, {4000.0, -4000.0}, nan, s_belgianLambert72)},
    {GridDefinition::Flanders1km, "Flanders 1km", GeoMetadata(154, 260, 0.0, 142000.0, 1000.0, nan, s_belgianLambert72)},
    {GridDefinition::CAMS, "CAMS", GeoMetadata(841, 1801, -30.0, 29.95, 0.05, nan, s_epsg4326)},

    {GridDefinition::Chimere05deg, "Chimere 0.5 degrees", GeoMetadata(46, 67, -10.750, 34.750, 0.5, nan, s_epsg4326)},
    {GridDefinition::Chimere01deg, "Chimere 0.1 degree", GeoMetadata(48, 111, -1.05, 48.75, 0.1, nan, s_epsg4326)},
    {GridDefinition::Chimere005degLarge, "Chimere 0.05 degrees large", GeoMetadata(65, 121, 1.225, 48.675, 0.05, nan, s_epsg4326)},
    {GridDefinition::Chimere005degSmall, "Chimere 0.05 degrees small", GeoMetadata(53, 103, 2.125, 48.975, 0.05, nan, s_epsg4326)},
    {GridDefinition::Chimere0025deg, "Chimere 0.025 degrees", GeoMetadata(117, 206, 2.1125, 48.9875, 0.025, nan, s_epsg4326)},
    {GridDefinition::ChimereEmep, "Chimere EMEP", GeoMetadata(520, 1199, -30.0, 30.0, 0.1, nan, s_epsg4326)},
    {GridDefinition::ChimereCams, "Chimere CAMS", GeoMetadata(1040, 1199, -30.0, 30.0, {0.1, -0.05}, nan, s_epsg4326)},
    {GridDefinition::ChimereRio1, "Chimere rio1", GeoMetadata(108, 252, 2.44872, 50.60386, {0.0141, -0.00895}, nan, s_epsg4326)},
    {GridDefinition::ChimereRio4, "Chimere rio4", GeoMetadata(72, 80, 2.16672, 49.24346, {0.0564, -0.0358}, nan, s_epsg4326)},
    {GridDefinition::ChimereRio32, "Chimere rio32", GeoMetadata(78, 73, -10.46688, 35.20986, {0.4512, -0.2864}, nan, s_epsg4326)},
    {GridDefinition::SherpaEmep, "Sherpa EMEP", GeoMetadata(415, 521, -15.1, 30, {0.1, -0.1}, nan, s_epsg4326)},
    {GridDefinition::SherpaChimere, "Sherpa Chimere", GeoMetadata(448, 384, -10.5, 34, {0.125, -0.0625}, nan, s_epsg4326)},
    {GridDefinition::Quark1km, "Quark 1km", GeoMetadata(5420, 3921, 1082500, 1386500, {1000, -1000}, nan, s_epsg3035)},
}};

std::vector<GridDefinition> grids_for_model_grid(ModelGrid grid)
{
    switch (grid) {
    case ModelGrid::Vlops1km:
        return {GridDefinition::Vlops60km, GridDefinition::Vlops5km, GridDefinition::Vlops1km};
    case ModelGrid::Vlops250m:
        return {GridDefinition::Vlops60km, GridDefinition::Vlops5km, GridDefinition::Vlops250m};
    case ModelGrid::Chimere05deg:
        return {GridDefinition::Chimere05deg};
    case ModelGrid::Chimere01deg:
        return {GridDefinition::Chimere01deg};
    case ModelGrid::Chimere005degLarge:
        return {GridDefinition::Chimere005degLarge};
    case ModelGrid::Chimere005degSmall:
        return {GridDefinition::Chimere005degSmall};
    case ModelGrid::Chimere0025deg:
        return {GridDefinition::Chimere0025deg};
    case ModelGrid::ChimereEmep:
        return {GridDefinition::ChimereEmep};
    case ModelGrid::ChimereCams:
        return {GridDefinition::ChimereCams};
    case ModelGrid::ChimereRio1:
        return {GridDefinition::ChimereRio1};
    case ModelGrid::ChimereRio4:
        return {GridDefinition::ChimereRio4};
    case ModelGrid::ChimereRio32:
        return {GridDefinition::ChimereRio32};
    case ModelGrid::SherpaEmep:
        return {GridDefinition::SherpaEmep};
    case ModelGrid::SherpaChimere:
        return {GridDefinition::SherpaChimere};
    case ModelGrid::Quark1km:
        return {GridDefinition::Quark1km};
    default:
        break;
    }

    throw RuntimeError("Invalid model grid provided");
}

const GridData& grid_data(GridDefinition grid) noexcept
{
    assert(enum_value(grid) < truncate<std::underlying_type_t<GridDefinition>>(s_gridData.size()));
    return s_gridData[enum_value(grid)];
}

}
