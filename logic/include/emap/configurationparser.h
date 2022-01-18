#pragma once

#include "emap/runconfiguration.h"
#include "infra/filesystem.h"

#include <optional>

namespace emap {

CountryInventory parse_countries(const fs::path& countrySpec);
SectorInventory parse_sectors(const fs::path& sectorSpec, const fs::path& conversionSpec);
PollutantInventory parse_pollutants(const fs::path& pollutantSpec, const fs::path& conversionSpec);

std::optional<RunConfiguration> parse_run_configuration_file(const fs::path& config);
std::optional<RunConfiguration> parse_run_configuration(std::string_view configContents, const fs::path& basePath); // used for testing

}
