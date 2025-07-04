﻿#include "spatialpatterninventory.h"

#include "emap/gridprocessing.h"
#include "emap/inputparsers.h"
#include "emap/pollutant.h"
#include "emap/runconfiguration.h"
#include "emap/sectorinventory.h"

#include "infra/enumutils.h"
#include "infra/gdal.h"
#include "infra/log.h"
#include "infra/string.h"

#include "gdx/algo/sum.h"
#include "gdx/denserasterio.h"

#include <deque>
#include <tuple>

namespace emap {

using namespace inf;
namespace gdal = inf::gdal;

static std::set<date::year> scan_available_years(const fs::path& spatialPatternPath)
{
    std::set<date::year> years;

    for (const auto& dirEntry : std::filesystem::directory_iterator(spatialPatternPath)) {
        if (dirEntry.is_directory()) {
            if (auto year = str::to_int32(dirEntry.path().stem().string()); year.has_value()) {
                years.emplace(*year);
            }
        }
    }

    return years;
}

static std::deque<date::year> create_years_sequence(date::year startYear, std::set<date::year> availableYears)
{
    /*
        When looking for spatial patterns, the following sequence should be used to look for a pattern
        - the requested year
        - the requested year - 1
        - the requested year + 1
        - the requested year - 2
        - the requested year + 2
        - and so on ...
    */

    std::deque<date::year> years;

    if (availableYears.count(startYear) > 0) {
        years.push_back(startYear);
        availableYears.erase(startYear);
    }

    int32_t currentOffset = -1;
    auto currentYear      = startYear;

    while (!availableYears.empty()) {
        currentYear = startYear + date::years(currentOffset);

        if (availableYears.count(currentYear) > 0) {
            years.push_back(currentYear);
            availableYears.erase(currentYear);
        }

        currentOffset *= -1;
        if (currentOffset < 0) {
            currentOffset -= 1;
        }
    }

    return years;
}

SpatialPatternTableCache::SpatialPatternTableCache(const RunConfiguration& cfg) noexcept
: _cfg(cfg)
{
}

const SpatialPatternData* SpatialPatternTableCache::get_data(const fs::path& path, const EmissionIdentifier& id, bool allowPollutantMismatch)
{
    std::scoped_lock lock(_mutex);
    if (_patterns.count(path) == 0) {
        auto patterns = std::make_unique<std::vector<SpatialPatternData>>(parse_spatial_pattern_flanders(path, _cfg));
        _patterns.emplace(path, std::move(patterns));
    }

    if (allowPollutantMismatch) {
        return find_data_for_sector(*_patterns.at(path), id.sector);
    } else {
        return find_data_for_id(*_patterns.at(path), id);
    }
}

const SpatialPatternData* SpatialPatternTableCache::find_data_for_id(const std::vector<SpatialPatternData>& list, const EmissionIdentifier& emissionId) const noexcept
{
    return inf::find_in_container(list, [&](const SpatialPatternData& src) {
        return src.id == emissionId;
    });
}

const SpatialPatternData* SpatialPatternTableCache::find_data_for_sector(const std::vector<SpatialPatternData>& list, const EmissionSector& sector) const noexcept
{
    return inf::find_in_container(list, [&](const SpatialPatternData& src) {
        return src.id.sector == sector;
    });
}

SpatialPatternInventory::SpatialPatternInventory(const RunConfiguration& cfg)
: _cfg(cfg)
, _spatialPatternCamsRegex("CAMS_emissions_REG-\\w+v\\d+.\\d+_(\\d{4})_(\\w+)_([A-Z]{1}_[^_]+|[1-6]{1}[^_]+)")
, _spatialPatternCeipRegex("(\\w+)_([A-Z]{1}_[^_]+|[1-6]{1}[^_]+)_(\\d{4})_GRID_(\\d{4})")
, _spatialPatternBelgium1Regex("Emissies per km2 (?:excl|incl) puntbrongegevens_(\\d{4})_([\\w,]+)")
, _spatialPatternBelgium2Regex("Emissie per km2_met NFR_([\\w ,]+) (\\d{4})_(\\w+) (\\d{4})")
, _flandersCache(cfg)
{
}

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_cams(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().string();

    if (std::regex_match(filename, baseMatch, _spatialPatternCamsRegex)) {
        try {
            // const auto year      = date::year(str::to_int32_value(baseMatch[1].str()));
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[2].str());
            const auto sector    = _cfg.sectors().sector_from_string(baseMatch[3].str());

            return SpatialPatternFile{
                SpatialPatternFile::Source::Cams,
                path,
                pollutant,
                sector};
        } catch (const std::exception& e) {
            Log::debug("Unexpected CAMS spatial pattern filename: {} ({})", e.what(), path);
        }
    }

    return {};
}

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_ceip(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().string();

    if (std::regex_match(filename, baseMatch, _spatialPatternCeipRegex)) {
        try {
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[1].str());
            const auto sector    = _cfg.sectors().sector_from_string(baseMatch[2].str());
            // const auto reportYear = date::year(str::to_int32_value(baseMatch[3].str()));
            // const auto year = date::year(str::to_int32_value(baseMatch[4].str()));

            return SpatialPatternFile{
                SpatialPatternFile::Source::Ceip,
                path,
                pollutant,
                sector};
        } catch (const std::exception& e) {
            Log::debug("Unexpected CEIP spatial pattern filename: {} ({})", e.what(), path);
        }
    }

    return {};
}

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_flanders(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().string();

    if (std::regex_match(filename, baseMatch, _spatialPatternBelgium1Regex)) {
        try {
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[2].str());

            return SpatialPatternFile{SpatialPatternFile::Source::FlandersTable, path, pollutant, {}};
        } catch (const std::exception& e) {
            Log::debug("Unexpected spatial pattern filename: {} ({})", e.what(), path);
        }
    } else if (std::regex_match(filename, baseMatch, _spatialPatternBelgium2Regex)) {
        try {
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[1].str());

            return SpatialPatternFile{SpatialPatternFile::Source::FlandersTable, path, pollutant, {}};
        } catch (const std::exception& e) {
            Log::debug("Unexpected spatial pattern filename: {} ({})", e.what(), path);
        }
    }

    return {};
}

std::vector<SpatialPatternInventory::SpatialPatterns> SpatialPatternInventory::scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const
{
    std::vector<SpatialPatterns> result;

    const auto camsPath = spatialPatternPath / "CAMS";
    const auto ceipPath = spatialPatternPath / "CEIP";

    auto ceipYears = scan_available_years(ceipPath);

    auto availableYears = scan_available_years(camsPath);
    availableYears.insert(ceipYears.begin(), ceipYears.end());

    auto yearsSequence = create_years_sequence(startYear, availableYears);
    while (!yearsSequence.empty()) {
        SpatialPatterns patternsForYear;
        patternsForYear.year = yearsSequence.front();

        {
            // Scan cams files
            const auto currentPath = camsPath / std::to_string(static_cast<int>(yearsSequence.front()));
            if (fs::is_directory(currentPath)) {
                for (const auto& dirEntry : std::filesystem::directory_iterator(currentPath)) {
                    if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".tif") {
                        if (const auto source = identify_spatial_pattern_cams(dirEntry.path()); source.has_value()) {
                            patternsForYear.spatialPatterns.push_back(*source);
                        }
                    }
                }
            }
        }

        {
            // Scan ceip files
            const auto currentPath = ceipPath / std::to_string(static_cast<int>(yearsSequence.front()));
            if (fs::is_directory(currentPath)) {
                for (const auto& dirEntry : std::filesystem::directory_iterator(currentPath)) {
                    if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".txt") {
                        if (const auto source = identify_spatial_pattern_ceip(dirEntry.path()); source.has_value()) {
                            patternsForYear.spatialPatterns.push_back(*source);
                        }
                    }
                }
            }
        }

        if (!patternsForYear.spatialPatterns.empty()) {
            result.push_back(std::move(patternsForYear));
        }

        yearsSequence.pop_front();
    }

    return result;
}

std::vector<SpatialPatternInventory::SpatialPatterns> SpatialPatternInventory::scan_dir_flanders(date::year startYear, const fs::path& spatialPatternPath) const
{
    std::vector<SpatialPatterns> result;

    if (fs::exists(spatialPatternPath)) {
        auto yearsSequence = create_years_sequence(startYear, scan_available_years(spatialPatternPath));
        while (!yearsSequence.empty()) {
            SpatialPatterns patternsForYear;
            patternsForYear.year = yearsSequence.front();

            const auto currentPath = spatialPatternPath / std::to_string(static_cast<int>(yearsSequence.front()));
            if (fs::is_directory(currentPath)) {
                for (const auto& dirEntry : std::filesystem::directory_iterator(currentPath)) {
                    if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".xlsx") {
                        if (const auto source = identify_spatial_pattern_flanders(dirEntry.path()); source.has_value()) {
                            patternsForYear.spatialPatterns.push_back(*source);
                        }
                    }
                }
            }

            if (!patternsForYear.spatialPatterns.empty()) {
                result.push_back(std::move(patternsForYear));
            }

            yearsSequence.pop_front();
        }
    }

    return result;
}

static fs::path reporing_dir(date::year reportYear)
{
    return file::u8path(fmt::format("reporting_{}", static_cast<int>(reportYear)));
}

void SpatialPatternInventory::scan_dir(date::year reportingYear, date::year startYear, const fs::path& spatialPatternPath)
{
    std::vector<SpatialPatternSource> result;

    _exceptions = parse_spatial_pattern_exceptions(_cfg.spatial_pattern_exceptions());
    // remove all the exceptions that are not relevant for the configured year
    inf::remove_from_container(_exceptions, [=](const SpatialPatternException& ex) {
        return !ex.yearRange.contains(startYear);
    });

    _spatialPatternsRest = scan_dir_rest(startYear, spatialPatternPath / "rest" / reporing_dir(reportingYear));
    _countrySpecificSpatialPatterns.emplace(country::BEF, scan_dir_flanders(startYear, spatialPatternPath / "bef" / reporing_dir(reportingYear)));
}

std::optional<SpatialPatternSource> SpatialPatternInventory::search_spatial_pattern_within_year(const Country& country,
                                                                                                const Pollutant& pollutant,
                                                                                                const Pollutant& polToReport,
                                                                                                const EmissionSector& sector,
                                                                                                const EmissionSector& sectorToReport,
                                                                                                date::year year,
                                                                                                const std::vector<SpatialPatternFile>& patterns) const
{
    bool isException = sector != sectorToReport;

    auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
        bool pollutantMatch = spf.pollutant == pollutant;
        bool sectorMatch    = spf.sector.is_valid() ? spf.sector == sector : true;
        return pollutantMatch && sectorMatch;
    });

    if (iter != patterns.end()) {
        if (iter->source == SpatialPatternFile::Source::Cams) {
            return SpatialPatternSource::create_from_cams(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, sector, pollutant), year, isException);
        } else if (iter->source == SpatialPatternFile::Source::Ceip) {
            return SpatialPatternSource::create_from_ceip(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, sector, pollutant), year, isException);
        } else if (iter->source == SpatialPatternFile::Source::FlandersTable) {
            return SpatialPatternSource::create_from_flanders(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, sector, pollutant), year, isException);
        }

        throw std::logic_error("Unhandled spatial pattern source type");
    }

    if (sector.type() == EmissionSector::Type::Nfr) {
        // No matching spatial pattern for nfr sector
        // Check if there is one for the corresponding gnfr sector
        EmissionSector gnfrSector(sector.gnfr_sector());
        iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
            return spf.pollutant == pollutant && spf.sector == gnfrSector;
        });
    }

    if (iter != patterns.end()) {
        if (iter->source == SpatialPatternFile::Source::Cams) {
            return SpatialPatternSource::create_from_cams(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, iter->sector, iter->pollutant), year, isException);
        } else if (iter->source == SpatialPatternFile::Source::Ceip) {
            return SpatialPatternSource::create_from_ceip(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, iter->sector, iter->pollutant), year, isException);
        } else if (iter->source == SpatialPatternFile::Source::FlandersTable) {
            return SpatialPatternSource::create_from_flanders(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, iter->sector, iter->pollutant), year, isException);
        }

        throw std::logic_error("Unhandled spatial pattern source type");
    }

    return {};
}

std::optional<SpatialPatternInventory::SpatialPatternException> SpatialPatternInventory::find_pollutant_exception(const EmissionIdentifier& emissionId) const noexcept
{
    auto exception = find_in_container_optional(_exceptions, [&emissionId](const SpatialPatternException& ex) {
        return ex.emissionId == emissionId && (!ex.viaSector.has_value());
    });

    if (!exception.has_value() && emissionId.sector.type() == EmissionSector::Type::Nfr) {
        // See if there is an entry on gnfr level
        exception = find_pollutant_exception(convert_emission_id_to_gnfr_level(emissionId));
    }

    return exception;
}

std::optional<SpatialPatternInventory::SpatialPatternException> SpatialPatternInventory::find_sector_exception(const EmissionIdentifier& emissionId) const noexcept
{
    // Find the exception that has a "viaNFR" of "viaGNFR" configured
    return find_in_container_optional(_exceptions, [&emissionId](const SpatialPatternException& ex) {
        return ex.emissionId == emissionId && ex.viaSector.has_value();
    });
}

std::optional<SpatialPattern> SpatialPatternInventory::find_spatial_pattern_exception(const EmissionIdentifier& emissionId,
                                                                                      const CountryCellCoverage& countryCoverage,
                                                                                      const Pollutant& pollutantToReport,
                                                                                      const EmissionSector& sectorToReport,
                                                                                      bool checkContents,
                                                                                      bool& patternAvailableButWithoutData) const
{
    std::optional<SpatialPattern> result;

    if (auto exception = find_pollutant_exception(emissionId); exception.has_value()) {
        assert(!exception->viaSector.has_value());
        result         = SpatialPattern(source_from_exception(*exception, pollutantToReport, sectorToReport, _cfg.year()));
        result->raster = get_pattern_raster(result->source, countryCoverage, checkContents);

        if (result->raster.empty()) {
            patternAvailableButWithoutData = true;
            result.reset();
        }
    }

    return result;
}

SpatialPatternSource SpatialPatternInventory::source_from_exception(const SpatialPatternException& ex, const Pollutant& pollutantToReport, const EmissionSector& sectorToReport, date::year year)
{
    const EmissionIdentifier emissionId(ex.emissionId.country, sectorToReport, pollutantToReport);

    switch (ex.type) {
    case SpatialPatternException::Type::Tif:
        return SpatialPatternSource::create_from_raster(ex.spatialPattern, emissionId, ex.emissionId, true);
    case SpatialPatternException::Type::Cams:
        return SpatialPatternSource::create_from_cams(ex.spatialPattern, emissionId, ex.emissionId, year, true);
    case SpatialPatternException::Type::Ceip:
        return SpatialPatternSource::create_from_ceip(ex.spatialPattern, emissionId, ex.emissionId, year, true);
    case SpatialPatternException::Type::FlandersTable:
        return SpatialPatternSource::create_from_flanders(ex.spatialPattern, emissionId, ex.emissionId, year, true);
    }

    throw RuntimeError("Invalid spatial pattern exception type");
}

static gdx::DenseRaster<double> extract_country_from_pattern(const gdx::DenseRaster<double>& spatialPattern, const CountryCellCoverage& countryCoverage, bool checkContents)
{
    auto raster = extract_country_from_raster(spatialPattern, countryCoverage);

    /*bool containsOnlyBorderCells = !std::any_of(countryCoverage.cells.begin(), countryCoverage.cells.end(), [](const CountryCellCoverage::CellInfo& cell) {
        return cell.coverage == 1.0;
    });*/

    if (checkContents) {
        bool containsData = false;

        // This behavior tries to be smart and see the data is only present in the border cells
        // Has unpleasant side effect on low resolutions for small countries. Needs to be improved

        /*if (containsOnlyBorderCells) {
            containsData = std::any_of(countryCoverage.cells.begin(), countryCoverage.cells.end(), [&](const CountryCellCoverage::CellInfo& cell) {
                return raster[cell.countryGridCell] > 0.0;
            });
        } else {
            containsData = std::any_of(countryCoverage.cells.begin(), countryCoverage.cells.end(), [&](const CountryCellCoverage::CellInfo& cell) {
                return cell.coverage == 1.0 && raster[cell.countryGridCell] > 0.0;
            });
        }*/

        containsData = std::any_of(countryCoverage.cells.begin(), countryCoverage.cells.end(), [&](const CountryCellCoverage::CellInfo& cell) {
            return raster[cell.countryGridCell] > 0.0;
        });

        if (containsData) {
            normalize_raster(raster);
        } else {
            raster = {};
        }
    } else {
        normalize_raster(raster);
    }

    return raster;
}

static gdx::DenseRaster<double> read_country_from_pattern(const fs::path& spatialPatternPath, const CountryCellCoverage& countryCoverage, bool checkContents)
{
    auto raster = gdx::resample_raster(gdx::read_dense_raster<double>(spatialPatternPath), countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average);

    if (checkContents) {
        bool containsData = std::any_of(raster.begin(), raster.end(), [](double val) {
            return val > 0;
        });

        if (containsData) {
            normalize_raster(raster);
        } else {
            raster = {};
        }
    } else {
        normalize_raster(raster);
    }

    return raster;
}

gdx::DenseRaster<double> SpatialPatternInventory::get_pattern_raster(const SpatialPatternSource& src, const CountryCellCoverage& countryCoverage, bool checkContents) const
{
    switch (src.type) {
    case SpatialPatternSource::Type::SpatialPatternCEIP:
        return extract_country_from_pattern(parse_spatial_pattern_ceip(src.path, src.usedEmissionId, _cfg), countryCoverage, checkContents);
    case SpatialPatternSource::Type::SpatialPatternFlanders: {
        const auto* spatialPatternData = _flandersCache.get_data(src.path, src.usedEmissionId, src.isException);
        if (spatialPatternData != nullptr) {
            auto result = gdx::resample_raster(spatialPatternData->raster, countryCoverage.outputSubgridExtent, gdal::ResampleAlgorithm::Average);
            if ((!checkContents) || gdx::sum(result) > 0.0) {
                normalize_raster(result);
                return result;
            }
        }

        return {};
    }
    case SpatialPatternSource::Type::SpatialPatternCAMS:
        [[fallthrough]];
    case SpatialPatternSource::Type::Raster:
        if (countryCoverage.country == country::BEF) {
            // Flanders should never be extracted, there is no data for other countries
            // no ratio will be applied to the country borders
            return read_country_from_pattern(src.path, countryCoverage, checkContents);
        } else {
            return extract_country_from_pattern(gdx::read_dense_raster<double>(src.path), countryCoverage, checkContents);
        }
    default:
        break;
    }

    throw std::logic_error("Unhandled spatial pattern type");
}

std::optional<SpatialPattern> SpatialPatternInventory::find_spatial_pattern(const EmissionIdentifier& emissionId,
                                                                            const CountryCellCoverage& countryCoverage,
                                                                            const std::vector<SpatialPatterns>& patterns,
                                                                            const Pollutant& pollutantToReport,
                                                                            const EmissionSector& sectorToReport,
                                                                            bool checkContents,
                                                                            bool& patternAvailableButWithoutData) const
{
    for (auto& [year, patterns] : patterns) {
        if (auto source = search_spatial_pattern_within_year(emissionId.country, emissionId.pollutant, pollutantToReport, emissionId.sector, sectorToReport, year, patterns); source.has_value()) {
            SpatialPattern result(*source);
            result.raster = get_pattern_raster(*source, countryCoverage, checkContents);

            if (result.raster.empty()) {
                patternAvailableButWithoutData = true;
            } else {
                return result;
            }
        }
    }

    return {};
}

SpatialPattern SpatialPatternInventory::get_spatial_pattern_impl(EmissionIdentifier emissionId, const CountryCellCoverage& countryCoverage, bool checkContents) const
{
    const auto sectorToReport           = emissionId.sector;
    bool patternAvailableButWithoutData = false;

    // Check if we should find this pattern via another sector
    if (auto exception = find_sector_exception(emissionId); exception.has_value()) {
        assert(exception->viaSector.has_value());
        emissionId = emissionId.with_sector(*exception->viaSector);
    }

    auto countrySpecificIter = _countrySpecificSpatialPatterns.find(emissionId.country);
    const auto& patterns     = countrySpecificIter != _countrySpecificSpatialPatterns.end() ? countrySpecificIter->second : _spatialPatternsRest;

    // first check the exceptions
    if (auto exception = find_spatial_pattern_exception(emissionId, countryCoverage, emissionId.pollutant, sectorToReport, checkContents, patternAvailableButWithoutData); exception.has_value()) {
        return std::move(*exception);
    }

    // then the regular patterns
    if (auto exception = find_spatial_pattern(emissionId, countryCoverage, patterns, emissionId.pollutant, sectorToReport, checkContents, patternAvailableButWithoutData); exception.has_value()) {
        return std::move(*exception);
    }

    // Try the fallback pollutant
    if (auto fallbackPollutant = _cfg.pollutants().pollutant_fallback(emissionId.pollutant); fallbackPollutant.has_value()) {
        // A fallback is defined, search for the pattern

        const auto fallbackId = emissionId.with_pollutant(*fallbackPollutant);

        // first check the exceptions
        if (auto exception = find_spatial_pattern_exception(fallbackId, countryCoverage, emissionId.pollutant, sectorToReport, checkContents, patternAvailableButWithoutData); exception.has_value()) {
            return std::move(*exception);
        }

        // then the regular patterns
        if (auto exception = find_spatial_pattern(fallbackId, countryCoverage, patterns, emissionId.pollutant, sectorToReport, checkContents, patternAvailableButWithoutData); exception.has_value()) {
            return std::move(*exception);
        }
    }

    // last resort: uniform spread
    return SpatialPattern(SpatialPatternSource::create_with_uniform_spread(emissionId.country, emissionId.sector, emissionId.pollutant, patternAvailableButWithoutData));
}

SpatialPattern SpatialPatternInventory::get_spatial_pattern_checked(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage) const
{
    return get_spatial_pattern_impl(emissionId, countryCoverage, true);
}

SpatialPattern SpatialPatternInventory::get_spatial_pattern(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage) const
{
    return get_spatial_pattern_impl(emissionId, countryCoverage, false);
}

SpatialPatternInventory::SpatialPatternException::Type SpatialPatternInventory::exception_type_from_string(std::string_view str)
{
    if (str::iequals(str, "tif")) {
        return SpatialPatternException::Type::Tif;
    }

    if (str::iequals(str, "BEF")) {
        return SpatialPatternException::Type::FlandersTable;
    }

    if (str::iequals(str, "CEIP")) {
        return SpatialPatternException::Type::Ceip;
    }

    if (str::iequals(str, "CAMS")) {
        return SpatialPatternException::Type::Cams;
    }

    throw RuntimeError("Invalid spatial pattern exception type: {}", str);
}

std::vector<SpatialPatternInventory::SpatialPatternException> SpatialPatternInventory::parse_spatial_pattern_exceptions(const fs::path& exceptionsFile) const
{
    std::vector<SpatialPatternException> result;
    if (exceptionsFile.empty()) {
        return result;
    }

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(exceptionsFile);
    auto layer = ds.layer("Spatial disaggregation");

    auto colYear      = layer.required_field_index("Year");
    auto colPollutant = layer.required_field_index("pollutant_code");
    auto colCountry   = layer.required_field_index("country_iso_code");
    auto colGnfr      = layer.required_field_index("GNFR_code");
    auto colNfr       = layer.required_field_index("NFR_code");
    auto colPath      = layer.required_field_index("file_path");
    auto colType      = layer.required_field_index("type");
    auto colViaNfr    = layer.required_field_index("via_NFR");
    auto colViaGnfr   = layer.required_field_index("via_GNFR");

    int row = 0;
    for (const auto& feature : layer) {
        ++row;

        if (!feature.field_is_valid(0)) {
            continue; // skip empty lines
        }

        try {
            auto country   = _cfg.countries().country_from_string(feature.field_as<std::string_view>(colCountry));
            auto pollutant = _cfg.pollutants().pollutant_from_string(feature.field_as<std::string_view>(colPollutant));
            std::optional<EmissionSector> sector;
            if (feature.field_is_valid(colGnfr)) {
                sector = EmissionSector(_cfg.sectors().gnfr_sector_from_code_string(feature.field_as<std::string_view>(colGnfr)));
            }

            if (feature.field_is_valid(colNfr)) {
                sector = EmissionSector(_cfg.sectors().nfr_sector_from_string(feature.field_as<std::string_view>(colNfr)));
            }

            if (sector.has_value()) {
                SpatialPatternException ex;
                ex.yearRange      = parse_year_range(feature.field_as<std::string>(colYear));
                ex.emissionId     = EmissionIdentifier(country, *sector, pollutant);
                ex.spatialPattern = exceptionsFile.parent_path() / file::u8path(feature.field_as<std::string_view>(colPath));
                ex.type           = exception_type_from_string(feature.field_as<std::string_view>(colType));

                if (feature.field_is_valid(colViaNfr)) {
                    ex.viaSector = EmissionSector(_cfg.sectors().nfr_sector_from_string(feature.field_as<std::string_view>(colViaNfr)));
                }

                if (feature.field_is_valid(colViaGnfr)) {
                    ex.viaSector = EmissionSector(_cfg.sectors().gnfr_sector_from_string(feature.field_as<std::string_view>(colViaGnfr)));
                }

                result.push_back(ex);
            }
        } catch (const std::exception& e) {
            Log::warn("Invalid line ({}) in spatial pattern exceptions file: {}", row, e.what());
        }
    }

    return result;
}
}
