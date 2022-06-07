#pragma once

#include "emap/emissions.h"
#include "emap/spatialpatterndata.h"
#include "infra/filesystem.h"
#include "infra/range.h"

#include <date/date.h>
#include <optional>
#include <regex>
#include <unordered_map>

namespace emap {

class SectorInventory;
class PollutantInventory;
class RunConfiguration;
struct CountryCellCoverage;

class SpatialPatternTableCache
{
public:
    SpatialPatternTableCache(const RunConfiguration& cfg) noexcept;

    const SpatialPatternData* get_data(const fs::path& path, const EmissionIdentifier& id, bool allowPollutantMismatch);

private:
    const SpatialPatternData* find_data_for_id(const std::vector<SpatialPatternData>& list, const EmissionIdentifier& emissionId) const noexcept;
    const SpatialPatternData* find_data_for_sector(const std::vector<SpatialPatternData>& list, const EmissionSector& sector) const noexcept;

    std::mutex _mutex;
    const RunConfiguration& _cfg;
    std::map<fs::path, std::unique_ptr<std::vector<SpatialPatternData>>> _patterns;
};

class SpatialPatternInventory
{
public:
    SpatialPatternInventory(const RunConfiguration& cfg);

    void scan_dir(date::year reportingYear, date::year startYear, const fs::path& spatialPatternPath);

    /* Obtain the spatial pattern for the given identifier, checks if the country cells contain actual data */
    SpatialPattern get_spatial_pattern_checked(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage) const;

    /* Obtain the spatial pattern for the given identifier without checking the contents of the pattern for data */
    SpatialPattern get_spatial_pattern(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage) const;

private:
    struct SpatialPatternFile
    {
        enum class Source
        {
            Cams,
            Ceip,
            FlandersTable,
            Invalid,
        };

        Source source = Source::Invalid;
        fs::path path;
        Pollutant pollutant;
        EmissionSector sector;
    };

    struct SpatialPatterns
    {
        date::year year;
        std::vector<SpatialPatternFile> spatialPatterns;
    };

    struct SpatialPatternException
    {
        enum class Type
        {
            Tif,
            FlandersTable,
            Ceip,
            Cams,
        };

        inf::Range<date::year> yearRange;
        EmissionIdentifier emissionId;
        fs::path spatialPattern;
        Type type;
        std::optional<EmissionSector> viaSector;
    };

    std::vector<SpatialPatternException> parse_spatial_pattern_exceptions(const fs::path& exceptionsFile) const;
    std::optional<SpatialPatternSource> search_spatial_pattern_within_year(const Country& country,
                                                                           const Pollutant& pol,
                                                                           const Pollutant& polToReport,
                                                                           const EmissionSector& sector,
                                                                           const EmissionSector& sectorToReport,
                                                                           date::year year,
                                                                           const std::vector<SpatialPatternFile>& patterns) const;

    std::optional<SpatialPatternFile> identify_spatial_pattern_cams(const fs::path& path) const;
    std::optional<SpatialPatternFile> identify_spatial_pattern_ceip(const fs::path& path) const;
    std::optional<SpatialPatternFile> identify_spatial_pattern_flanders(const fs::path& path) const;
    std::vector<SpatialPatterns> scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const;
    std::vector<SpatialPatterns> scan_dir_flanders(date::year startYear, const fs::path& spatialPatternPath) const;

    std::optional<SpatialPattern> find_spatial_pattern_exception(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage, const Pollutant& pollutantToReport, const EmissionSector& sectorToReport, bool checkContents, bool& patternAvailableButWithoutData) const;
    std::optional<SpatialPattern> find_spatial_pattern(const EmissionIdentifier& emissionId, const CountryCellCoverage& countryCoverage, const std::vector<SpatialPatterns>& patterns, const Pollutant& pollutantToReport, const EmissionSector& sectorToReport, bool checkContents, bool& patternAvailableButWithoutData) const;

    SpatialPattern get_spatial_pattern_impl(EmissionIdentifier emissionId, const CountryCellCoverage& countryCoverage, bool checkContents) const;

    std::optional<SpatialPatternException> find_pollutant_exception(const EmissionIdentifier& emissionId) const noexcept;
    std::optional<SpatialPatternException> find_sector_exception(const EmissionIdentifier& emissionId) const noexcept;
    static SpatialPatternSource source_from_exception(const SpatialPatternException& ex, const Pollutant& pollutantToReport, const EmissionSector& emissionSectorToReport, date::year year);
    static SpatialPatternException::Type exception_type_from_string(std::string_view str);

    gdx::DenseRaster<double> get_pattern_raster(const SpatialPatternSource& src, const CountryCellCoverage& countryCoverage, bool checkContents) const;

    const RunConfiguration& _cfg;
    std::regex _spatialPatternCamsRegex;
    std::regex _spatialPatternCeipRegex;
    std::regex _spatialPatternBelgium1Regex;
    std::regex _spatialPatternBelgium2Regex;
    // Contains all the exceptions for the configured year
    std::vector<SpatialPatternException> _exceptions;
    // Contains all the available patterns, sorted by year of preference
    std::vector<SpatialPatterns> _spatialPatternsRest;
    std::unordered_map<Country, std::vector<SpatialPatterns>> _countrySpecificSpatialPatterns;
    mutable SpatialPatternTableCache _flandersCache;
};

}