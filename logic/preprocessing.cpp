#include "emap/preprocessing.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"

#include "infra/chrono.h"
#include "infra/log.h"

namespace emap {

using namespace inf;
using namespace std::string_literals;

static void process_spatial_pattern_directory(const fs::path& inputDir, const fs::path& countriesShape, const fs::path& outputDir, PreprocessingProgress::Callback progressCb)
{
    file::create_directory_if_not_exists(outputDir);

    if (!fs::is_directory(inputDir)) {
        throw RuntimeError("Spatial patterns path should be a directory: {}", inputDir);
    }

    if (!fs::is_regular_file(countriesShape)) {
        throw RuntimeError("Countries vector path should be a path to a file: {}", countriesShape);
    }

    std::vector<fs::path> pathsToProcess;
    for (const auto& fileEntry : fs::directory_iterator(inputDir)) {
        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".tif") {
            pathsToProcess.push_back(fileEntry.path());
        }
    }

    PreprocessingProgressInfo info;
    info.step = PreprocessingProgressInfo::Step::CountryExtraction;
    std::atomic<int64_t> current(0);

    for (const auto& filePath : pathsToProcess) {
        info.file = &filePath;

        extract_countries_from_raster(filePath, countriesShape, outputDir, [&](const GridProcessingProgress::Status& status) {
            if (progressCb) {
                info.country = status.payload().country;
                info.currentCell = status.current();
                info.cellCount   = status.total();
                return progressCb(PreprocessingProgress::Status(++current, pathsToProcess.size(), info));
            }

            return ProgressStatusResult::Continue;
        });
    }
}

void run_preprocessing(const fs::path& configPath, PreprocessingProgress::Callback progressCb)
{
    return run_preprocessing(parse_preprocessing_configuration_file(configPath), progressCb);
}

void run_preprocessing(const std::optional<PreprocessingConfiguration>& cfg, PreprocessingProgress::Callback progressCb)
{
    if (!cfg.has_value()) {
        Log::debug("No preprocessing requested");
        return;
    }

    if (auto spatialPatternsDir = cfg->spatial_patterns_path(); !spatialPatternsDir.empty()) {
        if (!fs::is_directory(spatialPatternsDir)) {
            throw RuntimeError("Spatial patterns path should be a directory: {}", spatialPatternsDir);
        }

        if (!fs::is_regular_file(cfg->countries_vector_path())) {
            throw RuntimeError("Countries vector path should be a path to a file: {}", cfg->countries_vector_path());
        }

        process_spatial_pattern_directory(spatialPatternsDir, cfg->countries_vector_path(), cfg->output_path(), progressCb);
    }
}

}
