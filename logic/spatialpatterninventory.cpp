#include "spatialpatterninventory.h"

#include "infra/enumutils.h"
#include "infra/log.h"
#include "infra/string.h"

#include <deque>
#include <tuple>

namespace emap {

using namespace inf;

static std::set<date::year> scan_available_years(const fs::path& spatialPatternPath)
{
    std::set<date::year> years;

    for (const auto& dirEntry : std::filesystem::directory_iterator(spatialPatternPath)) {
        if (dirEntry.is_directory()) {
            if (auto year = str::to_int32(dirEntry.path().stem().u8string()); year.has_value()) {
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

SpatialPatternInventory::SpatialPatternInventory()
: _spatialPatternCamsRegex("CAMS_emissions_REG-APv\\d+.\\d+_(\\d{4})_(\\w+)_([A-Z]{1}_[^_]+|[1-6]{1}[^_]+)")
, _spatialPatternExcelRegex("Emissies per km2 excl puntbrongegevens_(\\d{4})_(\\w+)")
{
}

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_cams(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().u8string();

    if (std::regex_match(filename, baseMatch, _spatialPatternCamsRegex)) {
        try {
            const auto year      = date::year(str::to_int32_value(baseMatch[1].str()));
            const auto pollutant = Pollutant::from_string(baseMatch[2].str());
            const auto sector    = EmissionSector::from_string(baseMatch[3].str());

            return SpatialPatternFile{
                path,
                pollutant,
                sector};
        } catch (const std::exception& e) {
            Log::debug("Unexpected spatial pattern filename: {} ({})", e.what(), path);
        }
    }

    return {};
}

std::optional<SpatialPatternInventory::SpatialPatternFile> SpatialPatternInventory::identify_spatial_pattern_excel(const fs::path& path) const
{
    std::smatch baseMatch;
    const std::string filename = path.stem().u8string();

    if (std::regex_match(filename, baseMatch, _spatialPatternExcelRegex)) {
        try {
            const auto year      = date::year(str::to_int32_value(baseMatch[1].str()));
            const auto pollutant = Pollutant::from_string(baseMatch[2].str());

            return SpatialPatternFile{path, pollutant, {}};
        } catch (const std::exception& e) {
            Log::debug("Unexpected spatial pattern filename: {} ({})", e.what(), path);
        }
    }

    return {};
}

std::vector<SpatialPatternInventory::SpatialPatterns> SpatialPatternInventory::scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const
{
    std::vector<SpatialPatterns> result;

    auto yearsSequence = create_years_sequence(startYear, scan_available_years(spatialPatternPath));
    while (!yearsSequence.empty()) {
        SpatialPatterns patternsForYear;
        patternsForYear.year = yearsSequence.front();

        const auto currentPath = spatialPatternPath / std::to_string(static_cast<int>(yearsSequence.front()));
        if (fs::is_directory(currentPath)) {
            for (const auto& dirEntry : std::filesystem::directory_iterator(currentPath)) {
                if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".tif") {
                    if (const auto source = identify_spatial_pattern_cams(dirEntry.path()); source.has_value()) {
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
                        if (const auto source = identify_spatial_pattern_excel(dirEntry.path()); source.has_value()) {
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

    _spatialPatternsRest = scan_dir_rest(startYear, spatialPatternPath / "rest" / reporing_dir(reportingYear));
    _countrySpecificSpatialPatterns.emplace(Country::Id::BEB, scan_dir_belgium(startYear, spatialPatternPath / "beb" / reporing_dir(reportingYear)));
    _countrySpecificSpatialPatterns.emplace(Country::Id::BEF, scan_dir_belgium(startYear, spatialPatternPath / "bef" / reporing_dir(reportingYear)));
    _countrySpecificSpatialPatterns.emplace(Country::Id::BEW, scan_dir_belgium(startYear, spatialPatternPath / "bew" / reporing_dir(reportingYear)));
}

SpatialPatternSource SpatialPatternInventory::get_spatial_pattern(Country country, Pollutant pol, EmissionSector sector) const
{
    auto sectorLevel = EmissionSector::Type::Nfr;

    if (auto countrySpecificIter = _countrySpecificSpatialPatterns.find(country); countrySpecificIter != _countrySpecificSpatialPatterns.end()) {
        for (auto& [year, patterns] : countrySpecificIter->second) {
            auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
                return spf.pollutant == pol;
            });

            if (iter != patterns.end()) {
                return SpatialPatternSource::create_from_table(iter->path, pol, sector, year, sectorLevel);
            }
        }
    } else {
        for (auto& [year, patterns] : _spatialPatternsRest) {
            auto iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
                return spf.pollutant == pol && spf.sector == sector;
            });

            if (iter != patterns.end()) {
                return SpatialPatternSource::create_from_raster(iter->path, pol, sector, year, sectorLevel);
            }

            if (sector.type() == EmissionSector::Type::Nfr) {
                // No matching spatial pattern for nfr sector
                // Check if there is one for the corresponding gnfr sector
                EmissionSector gnfrSector(sector.gnfr_sector());
                iter = std::find_if(patterns.begin(), patterns.end(), [&](const SpatialPatternFile& spf) {
                    return spf.pollutant == pol && spf.sector == gnfrSector;
                });

                if (iter != patterns.end()) {
                    sectorLevel = EmissionSector::Type::Gnfr;
                }
            }

            if (iter != patterns.end()) {
                return SpatialPatternSource::create_from_raster(iter->path, pol, sector, year, sectorLevel);
            }
        }
    }

    return SpatialPatternSource::create_with_uniform_spread(pol, sector);
}
}