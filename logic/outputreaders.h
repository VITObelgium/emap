#pragma once

#include "brnoutputentry.h"
#include "infra/filesystem.h"

#include <vector>

namespace emap {

std::vector<BrnOutputEntry> read_brn_output(const fs::path& path);

}