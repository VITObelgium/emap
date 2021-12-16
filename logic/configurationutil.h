#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"
#include "infra/string.h"

#include <date/date.h>
#include <fmt/format.h>

namespace emap {

inline fs::path spatial_pattern_filename(date::year year, const EmissionIdentifier& emissionId)
{
    return fs::u8path(fmt::format("spatial_pattern_{}_{}_{}_{}.tif", static_cast<int>(year), emissionId.country.code(), emissionId.sector.gnfr_name(), inf::str::lowercase(emissionId.pollutant.code())));
}

}
