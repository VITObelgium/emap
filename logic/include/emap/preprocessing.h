#pragma once

#include "emap/country.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

namespace emap {

using namespace inf;

class PreprocessingConfiguration;

struct PreprocessingProgressInfo
{
    enum class Step
    {
        CountryExtraction,
        Invalid,
    };

    std::string to_string() const
    {
        return fmt::format("{} [{}/{}]", Country(country).full_name(), currentCell, cellCount);
    }

    Step step = Step::Invalid;
    Country::Id country = Country::Id::Invalid;
    const fs::path* file = nullptr;

    int64_t currentCell = 0;
    int64_t cellCount   = 0;
};

using PreprocessingProgress = inf::ProgressTracker<PreprocessingProgressInfo>;

void run_preprocessing(const fs::path& configPath, PreprocessingProgress::Callback progressCb);
void run_preprocessing(const std::optional<PreprocessingConfiguration>& cfg, PreprocessingProgress::Callback progressCb);

}
