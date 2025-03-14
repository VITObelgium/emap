#pragma once

#include "emap/runconfiguration.h"
#include "emap/sectorparameterconfig.h"
#include "infra/filesystem.h"

#include <optional>
#include <unordered_map>

namespace emap {

SectorParameterConfiguration parse_sector_parameters_config(const fs::path& diffuseParametersPath,
                                                            SectorLevel level,
                                                            const PollutantInventory& polInv,
                                                            std::string_view outputSectorLevelName);

CountryInventory parse_countries(const fs::path& countrySpec);
SectorInventory parse_sectors(const fs::path& sectorSpec, const fs::path& conversionSpec, const fs::path& ignoreSpec, const CountryInventory& countries);
PollutantInventory parse_pollutants(const fs::path& pollutantSpec, const fs::path& conversionSpec, const fs::path& ignoreSpec, const CountryInventory& countries);
std::unordered_map<NfrId, std::string> parse_sector_mapping(const fs::path& mappingSpec, const SectorInventory& inv, const std::string& outputLevel);

RunConfiguration parse_run_configuration_file(const fs::path& config);
RunConfiguration parse_run_configuration(std::string_view configContents, const fs::path& basePath); // used for testing

}
