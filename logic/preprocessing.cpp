#include "emap/preprocessing.h"

#include "emap/configurationparser.h"
#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"

#include "infra/chrono.h"
#include "infra/enumutils.h"
#include "infra/gdalio.h"
#include "infra/log.h"
#include "infra/math.h"

#include <oneapi/tbb/parallel_for_each.h>
#include <regex>

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

static void process_spatial_pattern_directory(const fs::path& inputDir, date::year year, const fs::path& countriesVector, const fs::path& outputDir, const PreprocessingProgress::Callback& progressCb)
{
    file::create_directory_if_not_exists(outputDir);

    if (!fs::is_directory(inputDir)) {
        throw RuntimeError("Spatial patterns path should be a directory: {}", inputDir);
    }

    if (!fs::is_regular_file(throw_if_not_exists(countriesVector))) {
        throw RuntimeError("Countries vector path should be a path to a file: {}", countriesVector);
    }

    // Capture groups:
    // [1]: pollutant
    // [2]: Gnfr: sector
    const std::regex notBefRegex(fmt::format(".+_{}_(\\w*)_([A-M]_\\w*)", static_cast<int>(year)));

    // Capture groups:
    // [1]: pollutant
    const std::regex befRegex(fmt::format(".+_{}_(\\w+)", static_cast<int>(year)));

    struct ProcessData
    {
        ProcessData() = default;
        ProcessData(fs::path path_, Pollutant pol_, GnfrSector sector_)
        : path(std::move(path_))
        , pollutant(pol_)
        , sector(sector_)
        {
        }

        fs::path path;
        Pollutant pollutant;
        GnfrSector sector;
    };

    struct ProcessDataBEF
    {
        ProcessDataBEF() = default;
        ProcessDataBEF(fs::path path_, Pollutant pol_)
        : path(std::move(path_))
        , pollutant(pol_)
        {
        }

        fs::path path;
        Pollutant pollutant;
    };

    std::vector<ProcessData> pathsToProcessNotBef;
    std::smatch baseMatch;
    for (const auto& fileEntry : fs::directory_iterator(inputDir / "not_bef")) {
        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".tif") {
            const auto filename = fileEntry.path().stem().u8string();
            if (std::regex_match(filename, baseMatch, notBefRegex)) {
                const auto pollutant = Pollutant::from_string(baseMatch[1].str());
                const auto gnfr      = gnfr_sector_from_string(baseMatch[2].str());
                pathsToProcessNotBef.emplace_back(fileEntry.path(), pollutant, gnfr);
            }
        }
    }

    std::vector<ProcessDataBEF> pathsToProcessBef;
    for (const auto& fileEntry : fs::directory_iterator(inputDir / "bef")) {
        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".xlsx") {
            const auto filename = fileEntry.path().stem().u8string();
            if (std::regex_match(filename, baseMatch, befRegex)) {
                const auto pollutant = Pollutant::from_string(baseMatch[1].str());
                pathsToProcessBef.emplace_back(fileEntry.path(), pollutant);
            }
        }
    }

    if (pathsToProcessBef.empty()) {
        throw RuntimeError("No spatial pattern info available for flanders for year {}", static_cast<int>(year));
    }

    if (pathsToProcessNotBef.empty()) {
        throw RuntimeError("No spatial pattern info available for year {}", static_cast<int>(year));
    }

    static const char* fieldId = "Code3";

    auto extent = gdal::io::read_metadata(pathsToProcessNotBef.front().path);
    if (!extent.is_north_up()) {
        extent = read_raster_north_up(pathsToProcessNotBef.front().path).metadata();
    }

    PreprocessingProgress progress(known_countries_in_extent(extent, countriesVector, fieldId) + pathsToProcessNotBef.size() + pathsToProcessBef.size(), progressCb);

    const std::string filenamePrefix = fmt::format("spatial_pattern_{}", static_cast<int>(year));

    const auto countryCoverages = create_country_coverages(extent, countriesVector, fieldId, [&progress](const GridProcessingProgress::Status& status) {
        PreprocessingProgressInfo info;
        info.step    = PreprocessingProgressInfo::Step::CellBoundaries;
        info.country = status.payload();
        progress.set_payload(info);
        progress.tick();
        return ProgressStatusResult::Continue;
    });

    fs::create_directories(outputDir);

    tbb::parallel_for_each(pathsToProcessNotBef.begin(), pathsToProcessNotBef.end(), [&](const ProcessData& processData) {
        Log::info(processData.path.u8string());
        extract_countries_from_raster(
            processData.path, processData.sector, countryCoverages, outputDir, fmt::format("{}_{{}}_{}_{}.tif", filenamePrefix, processData.sector, processData.pollutant), [](const GridProcessingProgress::Status& /*status*/) {
                return ProgressStatusResult::Continue;
            });

        PreprocessingProgressInfo info;
        info.step = PreprocessingProgressInfo::Step::CountryExtraction;
        info.file = &processData.path;
        progress.set_payload(info);
        progress.tick_throw_on_cancel();
    });

    tbb::parallel_for_each(pathsToProcessBef.begin(), pathsToProcessBef.end(), [&](const ProcessDataBEF& processData) {
        const auto spatialPatternDataFlanders = parse_spatial_pattern_flanders(processData.path);
        for (const auto& spatData : spatialPatternDataFlanders) {
            assert(spatData.year == year);
            PreprocessingProgressInfo info;
            info.step    = PreprocessingProgressInfo::Step::CellBoundaries;
            info.country = Country(Country::Id::BEF).id();
            progress.set_payload(info);
            progress.tick_throw_on_cancel();

            gdx::DenseRaster<double> result(copy_metadata_replace_nodata(extent, math::nan<double>()), math::nan<float>());
            inf::gdal::io::warp_raster<double, double>(spatData.raster, spatData.raster.metadata(), result, result.metadata(), gdal::ResampleAlgorithm::Sum);
            gdx::write_raster(result, outputDir / fmt::format("{}_{}_{}_{}.tif", filenamePrefix, Country(Country::Id::BEF).code(), spatData.id.sector, processData.pollutant));
        }
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
        process_spatial_pattern_directory(spatialPatternsDir, cfg->year(), cfg->countries_vector_path(), cfg->output_path(), progressCb);
    }
}
}
