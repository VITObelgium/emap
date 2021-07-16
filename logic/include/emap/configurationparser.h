#pragma once

#include "emap/preprocessingconfiguration.h"
#include "emap/runconfiguration.h"
#include "infra/filesystem.h"

#include <optional>

namespace emap {

std::optional<PreprocessingConfiguration> parse_preprocessing_configuration_file(const fs::path& config);
std::optional<PreprocessingConfiguration> parse_preprocessing_configuration(std::string_view configContents);

std::optional<RunConfiguration> parse_run_configuration_file(const fs::path& config);
std::optional<RunConfiguration> parse_run_configuration(std::string_view configContents);

}
