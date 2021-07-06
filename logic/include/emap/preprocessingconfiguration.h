#pragma once

#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <fmt/core.h>
#include <optional>

namespace emap {

class PreprocessingConfiguration
{
public:
    PreprocessingConfiguration(
        const fs::path& spatialPatternsPath,
        const fs::path& countriesVector,
        const fs::path& outputPath);

    const fs::path& spatial_patterns_path() const noexcept;
    const fs::path& countries_vector_path() const noexcept;
    const fs::path& output_path() const noexcept;

private:
    fs::path _spatialPatternsPath;
    fs::path _countriesPath;
    fs::path _outputPath;
};

}
