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
        CellBoundaries,
        CountryExtraction,
        Invalid,
    };

    std::string to_string() const
    {
        if (step == Step::CellBoundaries) {
            return fmt::format("Calculating cell boundaries for {}", Country(country).full_name());
        }

        if (step == Step::CountryExtraction) {
            return fmt::format("Extracting countries from {}", file->stem().u8string());
        }

        return std::string(Country(country).full_name());
    }

    Step step            = Step::Invalid;
    Country::Id country  = Country::Id::Invalid;
    const fs::path* file = nullptr;
};

using PreprocessingProgress = inf::ProgressTracker<PreprocessingProgressInfo>;

void run_preprocessing(const fs::path& configPath, const PreprocessingProgress::Callback& progressCb);
void run_preprocessing(const std::optional<PreprocessingConfiguration>& cfg, const PreprocessingProgress::Callback& progressCb);

}
