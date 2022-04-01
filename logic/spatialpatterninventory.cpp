#include "spatialpatterninventory.h"

#include "emap/inputparsers.h"
#include "emap/pollutant.h"
#include "emap/runconfiguration.h"
#include "emap/sectorinventory.h"

#include "infra/enumutils.h"
#include "infra/gdal.h"
#include "infra/log.h"
#include "infra/string.h"

#include "gdx/algo/sum.h"

#include <deque>
#include <tuple>

namespace emap {

using namespace inf;

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

static Range<date::year> parse_year_range(std::string_view yearRange)
{
    auto splitted = str::split_view(yearRange, '-');
    if (splitted.size() == 1) {
        auto year = date::year(str::to_uint32_value(yearRange));
        return Range(year, year);
    } else if (splitted.size() == 2) {
        auto year1 = date::year(str::to_uint32_value(splitted[0]));
        auto year2 = date::year(str::to_uint32_value(splitted[1]));
        return Range(year1, year2);
    }

    throw RuntimeError("Invalid year range specification: {}", yearRange);
}

SpatialPatternTableCache::SpatialPatternTableCache(const RunConfiguration& cfg) noexcept
: _cfg(cfg)
{
}

const SpatialPatternData* SpatialPatternTableCache::get_data(const fs::path& path, const EmissionIdentifier& id)
{
    std::scoped_lock lock(_mutex);
    if (_patterns.count(path) == 0) {
        auto patterns = std::make_unique<std::vector<SpatialPatternData>>(parse_spatial_pattern_flanders(path, _cfg));
        _patterns.emplace(path, std::move(patterns));
    }

    return find_data_for_id(*_patterns.at(path), id);
}

const SpatialPatternData* SpatialPatternTableCache::find_data_for_id(const std::vector<SpatialPatternData>& list, const EmissionIdentifier& emissionId) const noexcept
{
    return inf::find_in_container(list, [&](const SpatialPatternData& src) {
        return src.id == emissionId;
    });
}

SpatialPatternInventory::SpatialPatternInventory(const RunConfiguration& cfg)
: _cfg(cfg)
, _spatialPatternCamsRegex("CAMS_emissions_REG-APv\\d+.\\d+_(\\d{4})_(\\w+)_([A-Z]{1}_[^_]+|[1-6]{1}[^_]+)")
, _spatialPatternCeipRegex("(\\w+)_([A-Z]{1}_[^_]+|[1-6]{1}[^_]+)_(\\d{4})_GRID_(\\d{4})")
, _spatialPatternBelgium1Regex("Emissies per km2 (?:excl|incl) puntbrongegevens_(\\d{4})_([\\w,]+)")
, _spatialPatternBelgium2Regex("Emissie per km2_met NFR_([\\w ,]+) (\\d{4})_(\\w+) (\\d{4})")
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

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_belgium(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().string();

    if (std::regex_match(filename, baseMatch, _spatialPatternBelgium1Regex)) {
        try {
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[2].str());

            return SpatialPatternFile{SpatialPatternFile::Source::SpreadSheet, path, pollutant, {}};
        } catch (const std::exception& e) {
            Log::debug("Unexpected spatial pattern filename: {} ({})", e.what(), path);
        }
    } else if (std::regex_match(filename, baseMatch, _spatialPatternBelgium2Regex)) {
        try {
            const auto pollutant = _cfg.pollutants().pollutant_from_string(baseMatch[1].str());

            return SpatialPatternFile{SpatialPatternFile::Source::SpreadSheet, path, pollutant, {}};
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

std::vector<SpatialPatternInventory::SpatialPatterns> SpatialPatternInventory::scan_dir_belgium(date::year startYear, const fs::path& spatialPatternPath) const
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
                        if (const auto source = identify_spatial_pattern_belgium(dirEntry.path()); source.has_value()) {
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
    return fs::u8path(fmt::format("reporting_{}", static_cast<int>(reportYear)));
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
    _countrySpecificSpatialPatterns.emplace(country::BEB, scan_dir_belgium(startYear, spatialPatternPath / "beb" / reporing_dir(reportingYear)));
    _countrySpecificSpatialPatterns.emplace(country::BEF, scan_dir_belgium(startYear, spatialPatternPath / "bef" / reporing_dir(reportingYear)));
    _countrySpecificSpatialPatterns.emplace(country::BEW, scan_dir_belgium(startYear, spatialPatternPath / "bew" / reporing_dir(reportingYear)));
}

// std::optional<SpatialPatternSource> SpatialPatternInventory::search_spatial_pattern_within_year(const Country& country,
//                                                                                                 const Pollutant& pol,
//                                                                                                 const Pollutant& polToReport,
//                                                                                                 const EmissionSector& sector,
//                                                                                                 date::year year,
//                                                                                                 const std::vector<SpatialPatternFile>& patterns) const
//
//{
//     auto sectorLevel = EmissionSector::Type::Nfr;
//
//     auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
//         return spf.pollutant == pol && spf.sector == sector;
//     });
//
//     if (iter != patterns.end()) {
//         if (iter->source == SpatialPatternFile::Source::Cams) {
//             return SpatialPatternSource::create_from_cams(iter->path, country, sector, polToReport, pol, year, sectorLevel);
//         } else if (iter->source == SpatialPatternFile::Source::Ceip) {
//             return SpatialPatternSource::create_from_cams(iter->path, country, sector, polToReport, pol, year, sectorLevel);
//         }
//
//         throw std::logic_error("Unhandled spatial pattern source type");
//     }
//
//     if (sector.type() == EmissionSector::Type::Nfr) {
//         // No matching spatial pattern for nfr sector
//         // Check if there is one for the corresponding gnfr sector
//         @ @-380, 97 + 382, 157 @ @std::optional<SpatialPatternInventory::SpatialPatternException> SpatialPatternIn
//                                // restore the nfr based id
//                                exception->emissionId = emissionId;
//     }
// }
//
// return exception;
// }

std::optional<SpatialPatternSource> SpatialPatternInventory::search_spatial_pattern_within_year(const Country& country,
                                                                                                const Pollutant& pollutant,
                                                                                                const Pollutant& polToReport,
                                                                                                const EmissionSector& sector,
                                                                                                const EmissionSector& sectorToReport,
                                                                                                date::year year,
                                                                                                const std::vector<SpatialPatternFile>& patterns) const
{
    auto sectorLevel = EmissionSector::Type::Nfr;
    bool isException = sector != sectorToReport;

    auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
        return spf.pollutant == pollutant && spf.sector == sector;
    });

    if (iter != patterns.end()) {
        if (iter->source == SpatialPatternFile::Source::Cams) {
            return SpatialPatternSource::create_from_cams(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, sector, pollutant), year, sectorLevel, isException);
        } else if (iter->source == SpatialPatternFile::Source::Ceip) {
            return SpatialPatternSource::create_from_ceip(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, sector, pollutant), year, sectorLevel, isException);
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

        if (iter != patterns.end()) {
            sectorLevel = EmissionSector::Type::Gnfr;
        }
    }

    if (iter != patterns.end()) {
        if (iter->source == SpatialPatternFile::Source::Cams) {
            return SpatialPatternSource::create_from_cams(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, iter->sector, iter->pollutant), year, sectorLevel, isException);
        } else if (iter->source == SpatialPatternFile::Source::Ceip) {
            return SpatialPatternSource::create_from_ceip(iter->path, EmissionIdentifier(country, sectorToReport, polToReport), EmissionIdentifier(country, iter->sector, iter->pollutant), year, sectorLevel, isException);
        }

        throw std::logic_error("Unhandled spatial pattern source type");
    }

    return {};
}

std::optional<SpatialPatternInventory::SpatialPatternException> SpatialPatternInventory::find_exception(const EmissionIdentifier& emissionId) const noexcept
{
    auto exception = find_in_container_optional(_exceptions, [&emissionId](const SpatialPatternException& ex) {
        return ex.emissionId == emissionId;
    });

    if (!exception.has_value()) {
        // Try the fallback pollutant
        if (auto fallbackPollutant = _cfg.pollutants().pollutant_fallback(emissionId.pollutant); fallbackPollutant.has_value()) {
            EmissionIdentifier fallbackId(emissionId.country, emissionId.sector, *fallbackPollutant);
            exception = find_in_container_optional(_exceptions, [&fallbackId](const SpatialPatternException& ex) {
                return ex.emissionId == fallbackId;
            });

            if (exception.has_value()) {
                // make sure the original pollutant gets used, not the fallback
                exception->emissionId.pollutant = emissionId.pollutant;
            }
        }
    }

    if (!exception.has_value() && emissionId.sector.type() == EmissionSector::Type::Nfr) {
        // See if there is an entry on gnfr level
        exception = find_exception(convert_emission_id_to_gnfr_level(emissionId));
        if (exception.has_value()) {
            // restore the nfr based id
            exception->emissionId = emissionId;
        }
    }

    return exception;
}

SpatialPatternSource SpatialPatternInventory::source_from_exception(const SpatialPatternException& ex, const Pollutant& pollutantToReport, const EmissionSector& sectorToReport, date::year year)
{
    const EmissionIdentifier emissionId(ex.emissionId.country, sectorToReport, pollutantToReport);

    switch (ex.type) {
    case SpatialPatternException::Type::Tif:
        return SpatialPatternSource::create_from_raster(ex.spatialPattern, emissionId, ex.emissionId, true);
    case SpatialPatternException::Type::Cams:
        return SpatialPatternSource::create_from_cams(ex.spatialPattern, emissionId, ex.emissionId, year, ex.emissionId.sector.type(), true);
    case SpatialPatternException::Type::Ceip:
        return SpatialPatternSource::create_from_ceip(ex.spatialPattern, emissionId, ex.emissionId, year, ex.emissionId.sector.type(), true);
    case SpatialPatternException::Type::FlandersTable:
        return SpatialPatternSource::create_from_table(ex.spatialPattern, emissionId, ex.emissionId, year, ex.emissionId.sector.type(), true);
    }

    throw RuntimeError("Invalid spatial pattern exception type");
}

SpatialPatternSource SpatialPatternInventory::get_country_specific_spatial_pattern(EmissionIdentifier emissionId, const std::vector<SpatialPatterns>& patterns, const EmissionSector& sectorToReport, SpatialPatternTableCache* cache) const
{
    // Currently only used for flanders
    bool patternAvailableButNodata = false;
    bool isException               = emissionId.sector != sectorToReport;

    for (auto& [year, patterns] : patterns) {
        auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
            return spf.pollutant == emissionId.pollutant;
        });

        if (iter != patterns.end()) {
            const auto* spatialPatternData = cache->get_data(iter->path, emissionId);
            if (spatialPatternData != nullptr) {
                if (gdx::sum(spatialPatternData->raster) > 0.0) {
                    return SpatialPatternSource::create_from_table(iter->path, EmissionIdentifier(emissionId.country, sectorToReport, emissionId.pollutant), spatialPatternData->id, year, EmissionSector::Type::Nfr, isException);
                } else {
                    patternAvailableButNodata = true;
                }
            }
        }

        // Try the fallback pollutant
        if (auto fallbackPollutant = _cfg.pollutants().pollutant_fallback(emissionId.pollutant); fallbackPollutant.has_value()) {
            // A fallback is defined, search for the pattern

            // first check the exceptions
            if (auto exception = find_exception(emissionId.with_pollutant(*fallbackPollutant)); exception.has_value()) {
                if (exception->viaSector.has_value()) {
                    // via sector is present: adjust the sector and continue regular search
                    emissionId = emissionId.with_sector(*exception->viaSector);
                } else {
                    return source_from_exception(*exception, emissionId.pollutant, sectorToReport, _cfg.year());
                }
            }

            // then the regular patterns
            iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
                return spf.pollutant == *fallbackPollutant;
            });

            if (iter != patterns.end()) {
                const auto* spatialPatternData = cache->get_data(iter->path, emissionId.with_pollutant(*fallbackPollutant));
                if (spatialPatternData != nullptr) {
                    if (gdx::sum(spatialPatternData->raster) > 0.0) {
                        return SpatialPatternSource::create_from_table(iter->path, EmissionIdentifier(emissionId.country, sectorToReport, emissionId.pollutant), spatialPatternData->id, year, EmissionSector::Type::Nfr, isException);
                    } else {
                        patternAvailableButNodata = true;
                    }
                }
            }
        }
    }

    // last resort: uniform spread
    return SpatialPatternSource::create_with_uniform_spread(emissionId.country, emissionId.sector, emissionId.pollutant, patternAvailableButNodata);
}

SpatialPatternSource SpatialPatternInventory::get_spatial_pattern(const EmissionIdentifier& emissionId, const std::vector<SpatialPatterns>& patterns, const EmissionSector& sectorToReport) const
{
    bool patternAvailableButNodata = false;

    for (auto& [year, patterns] : patterns) {
        if (auto source = search_spatial_pattern_within_year(emissionId.country, emissionId.pollutant, emissionId.pollutant, emissionId.sector, sectorToReport, year, patterns); source.has_value()) {
            return *source;
        } else {
            // Try the fallback pollutant
            if (auto fallbackPollutant = _cfg.pollutants().pollutant_fallback(emissionId.pollutant); fallbackPollutant.has_value()) {
                // A fallback is defined, search for the pattern

                // first check the exceptions
                if (auto exception = find_exception(emissionId.with_pollutant(*fallbackPollutant)); exception.has_value()) {
                    return source_from_exception(*exception, emissionId.pollutant, sectorToReport, _cfg.year());
                }

                // then the regular patterns
                if (auto source = search_spatial_pattern_within_year(emissionId.country, *fallbackPollutant, emissionId.pollutant, emissionId.sector, sectorToReport, year, patterns); source.has_value()) {
                    return *source;
                }
            }
        }
    }

    return SpatialPatternSource::create_with_uniform_spread(emissionId.country, emissionId.sector, emissionId.pollutant, patternAvailableButNodata);
}

SpatialPatternSource SpatialPatternInventory::get_spatial_pattern(const EmissionIdentifier& emissionId, SpatialPatternTableCache* cache) const
{
    auto id = emissionId;

    if (auto exception = find_exception(emissionId); exception.has_value()) {
        if (exception->viaSector.has_value()) {
            // via sector is present: adjust the sector and continue regular search
            id = emissionId.with_sector(*exception->viaSector);
        } else {
            return source_from_exception(*exception, emissionId.pollutant, emissionId.sector, _cfg.year());
        }
    }

    if (auto countrySpecificIter = _countrySpecificSpatialPatterns.find(id.country); countrySpecificIter != _countrySpecificSpatialPatterns.end()) {
        return get_country_specific_spatial_pattern(id, countrySpecificIter->second, emissionId.sector, cache);
    } else {
        return get_spatial_pattern(id, _spatialPatternsRest, emissionId.sector);
    }
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
                ex.yearRange      = parse_year_range(feature.field_as<std::string_view>(colYear));
                ex.emissionId     = EmissionIdentifier(country, *sector, pollutant);
                ex.spatialPattern = exceptionsFile.parent_path() / fs::u8path(feature.field_as<std::string_view>(colPath));
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