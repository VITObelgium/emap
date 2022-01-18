#pragma once

#include "emap/runconfiguration.h"
#include "infra/filesystem.h"

#include <optional>

namespace emap {

std::optional<RunConfiguration> parse_run_configuration_file(const fs::path& config);
std::optional<RunConfiguration> parse_run_configuration(std::string_view configContents, const fs::path& basePath); // used for testing

}
