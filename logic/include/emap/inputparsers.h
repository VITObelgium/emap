#pragma once

#include "infra/filesystem.h"

#include <vector>

namespace emap {

class Emissions;

Emissions parse_emissions(const fs::path& emissionsCsv);

}