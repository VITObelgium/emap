#include "emap/preprocessing.h"

#include "configurationutil.h"
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

void process_spatial_pattern_directory(const fs::path& inputDir, const PreprocessingConfiguration& cfg, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const PreprocessingProgress::Callback& progressCb)
{
    const auto outputDir = cfg.output_path();
    file::create_directory_if_not_exists(outputDir);

    if (!fs::is_directory(inputDir)) {
        throw RuntimeError("Spatial patterns path should be a directory: {}", inputDir);
    }

    if (!fs::is_regular_file(throw_if_not_exists(cfg.countries_vector_path()))) {
        throw RuntimeError("Countries vector path should be a path to a file: {}", cfg.countries_vector_path());
    }

    const auto year = cfg.year();

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
                const auto pollutant = pollutantInv.pollutant_from_string(baseMatch[1].str());
                const auto gnfr      = sectorInv.gnfr_sector_from_string(baseMatch[2].str());
                pathsToProcessNotBef.emplace_back(fileEntry.path(), pollutant, gnfr);
            }
        }
    }

    std::vector<ProcessDataBEF> pathsToProcessBef;
    for (const auto& fileEntry : fs::directory_iterator(inputDir / "bef")) {
        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".xlsx") {
            const auto filename = fileEntry.path().stem().u8string();
            if (std::regex_match(filename, baseMatch, befRegex)) {
                const auto pollutant = pollutantInv.pollutant_from_string(baseMatch[1].str());
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

    PreprocessingProgress progress(known_countries_in_extent(extent, cfg.countries_vector_path(), fieldId) + pathsToProcessNotBef.size() + pathsToProcessBef.size(), progressCb);

    const std::string filenamePrefix = fmt::format("spatial_pattern_{}", static_cast<int>(year));

    const auto countryCoverages = create_country_coverages(extent, cfg.countries_vector_path(), fieldId, [&progress](const GridProcessingProgress::Status& status) {
        PreprocessingProgressInfo info;
        info.step    = PreprocessingProgressInfo::Step::CellBoundaries;
        info.country = status.payload();
        progress.set_payload(info);
        progress.tick();
        return ProgressStatusResult::Continue;
    });

    fs::remove_all(outputDir);
    fs::create_directories(outputDir);

    tbb::parallel_for_each(pathsToProcessNotBef.begin(), pathsToProcessNotBef.end(), [&](const ProcessData& processData) {
        for (auto& [raster, country] : extract_countries_from_raster(processData.path, processData.sector, countryCoverages)) {
            if (country.included()) {
                gdx::write_raster(std::move(raster), outputDir / spatial_pattern_filename(year, EmissionIdentifier(country, EmissionSector(processData.sector), processData.pollutant)));
            }
        }

        PreprocessingProgressInfo info;
        info.step = PreprocessingProgressInfo::Step::CountryExtraction;
        info.file = &processData.path;
        progress.set_payload(info);
        progress.tick_throw_on_cancel();
    });

    tbb::parallel_for_each(pathsToProcessBef.begin(), pathsToProcessBef.end(), [&](const ProcessDataBEF& processData) {
        const auto spatialPatternDataFlanders = parse_spatial_pattern_flanders(processData.path, sectorInv, pollutantInv);
        for (const auto& spatData : spatialPatternDataFlanders) {
            assert(spatData.year == year);
            PreprocessingProgressInfo info;
            info.step    = PreprocessingProgressInfo::Step::CellBoundaries;
            info.country = Country(Country::Id::BEF).id();
            progress.set_payload(info);
            progress.tick_throw_on_cancel();

            gdx::write_raster(spatData.raster, outputDir / spatial_pattern_filename(year, EmissionIdentifier(Country(Country::Id::BEF), spatData.id.sector, processData.pollutant)));
        }
    });
}

}
