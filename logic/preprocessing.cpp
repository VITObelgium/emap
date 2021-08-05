#include "emap/preprocessing.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"
#include "gdx/denseraster.h"

#include "infra/chrono.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/log.h"

#include <oneapi/tbb/parallel_for_each.h>

namespace emap {

using namespace inf;
using namespace std::string_literals;

static fs::file_status throw_if_not_exists(const fs::path& path)
{
    const auto status = fs::status(path);
    if (!fs::exists(status)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return status;
}

static void process_spatial_pattern_directory(const fs::path& inputDir, const fs::path& countriesVector, const fs::path& outputDir, const PreprocessingProgress::Callback& progressCb)
{
    file::create_directory_if_not_exists(outputDir);

    if (!fs::is_directory(inputDir)) {
        throw RuntimeError("Spatial patterns path should be a directory: {}", inputDir);
    }

    if (!fs::is_regular_file(throw_if_not_exists(countriesVector))) {
        throw RuntimeError("Countries vector path should be a path to a file: {}", countriesVector);
    }

    std::vector<fs::path> pathsToProcess;
    for (const auto& fileEntry : fs::directory_iterator(inputDir)) {
        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".tif") {
            pathsToProcess.push_back(fileEntry.path());
        }
    }

    if (pathsToProcess.empty()) {
        return;
    }

    static const char* fieldId = "FID";

    auto extent = gdal::io::read_metadata(pathsToProcess.front());
    if (!extent.is_north_up()) {
        extent = read_raster_north_up(pathsToProcess.front()).metadata();
    }

    PreprocessingProgress progress(known_countries_in_extent(extent, countriesVector, fieldId), progressCb);

    const auto countyCoverages = create_country_coverages(extent, countriesVector, fieldId, [&progress](const GridProcessingProgress::Status& status) {
        PreprocessingProgressInfo info;
        info.step    = PreprocessingProgressInfo::Step::CellBoundaries;
        info.country = status.payload();
        progress.set_payload(info);
        progress.tick();
        return ProgressStatusResult::Continue;
    });

    progress.reset(pathsToProcess.size());
    tbb::parallel_for_each(pathsToProcess.begin(), pathsToProcess.end(), [&](const auto& filePath) {
        extract_countries_from_raster(filePath, countyCoverages, outputDir, [](const GridProcessingProgress::Status& /*status*/) {
            return ProgressStatusResult::Continue;
        });

        PreprocessingProgressInfo info;
        info.step = PreprocessingProgressInfo::Step::CountryExtraction;
        info.file = &filePath;
        progress.set_payload(info);
        progress.tick_throw_on_cancel();
    });
}

void run_preprocessing(const fs::path& configPath, const PreprocessingProgress::Callback& progressCb)
{
    return run_preprocessing(parse_preprocessing_configuration_file(configPath), progressCb);
}

void run_preprocessing(const std::optional<PreprocessingConfiguration>& cfg, const PreprocessingProgress::Callback& progressCb)
{
    if (!cfg.has_value()) {
        Log::debug("No preprocessing requested");
        return;
    }

    if (auto spatialPatternsDir = cfg->spatial_patterns_path(); !spatialPatternsDir.empty()) {
        process_spatial_pattern_directory(spatialPatternsDir, cfg->countries_vector_path(), cfg->output_path(), progressCb);
    }
}

}
