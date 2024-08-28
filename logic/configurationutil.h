#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"
#include "infra/string.h"

#include <date/date.h>
#include <fmt/format.h>

namespace emap {

using namespace inf;

inline fs::path spatial_pattern_filename(date::year year, const EmissionIdentifier& emissionId)
{
    return file::u8path(fmt::format("spatial_pattern_{}_{}_{}_{}.tif", static_cast<int>(year), emissionId.country.iso_code(), emissionId.sector.gnfr_name(), inf::str::lowercase(emissionId.pollutant.code())));
}

}
