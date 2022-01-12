#pragma once

#include "emap/country.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

namespace emap {

using namespace inf;

class SectorInventory;
class PollutantInventory;
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

void process_spatial_pattern_directory(const fs::path& inputDir, const PreprocessingConfiguration& cfg, const SectorInventory& sectorInventory, const PollutantInventory& polluantInventory, const PreprocessingProgress::Callback& progressCb);

}
