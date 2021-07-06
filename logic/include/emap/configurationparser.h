#pragma once

#include "infra/filesystem.h"
#include "emap/runconfiguration.h"
#include "emap/preprocessingconfiguration.h"

#include <optional>

namespace emap {

std::optional<PreprocessingConfiguration> parse_preprocessing_configuration_file(const fs::path& config);
std::optional<PreprocessingConfiguration> parse_preprocessing_configuration(std::string_view configContents);

RunConfiguration parse_run_configuration_file(const fs::path& config);
RunConfiguration parse_run_configuration(std::string_view configContents);

}
