#pragma once

#include "infra/filesystem.h"

namespace emap {

class RunConfiguration;

RunConfiguration parse_run_configuration(const fs::path& config);
RunConfiguration parse_run_configuration(std::string_view configContents);

}
