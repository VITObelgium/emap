#pragma once

#include "infra/filesystem.h"

#include <vector>

namespace emap {

std::vector<std::string> parse_emissions(const fs::path& emissionsCsv);

}