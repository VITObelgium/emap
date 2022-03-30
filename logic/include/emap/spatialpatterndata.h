#pragma once

#include "emap/emissions.h"
#include "gdx/denseraster.h"

#include <date/date.h>

namespace emap {

struct SpatialPatternData
{
    date::year year;
    EmissionIdentifier id;
    gdx::DenseRaster<double> raster;
};

}