#pragma once

#include "infra/filesystem.h"
#include "infra/log.h"

namespace emap {

int debug_grids(const fs::path& runConfigPath, inf::Log::Level logLevel);

}
