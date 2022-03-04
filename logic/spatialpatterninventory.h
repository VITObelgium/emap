#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"
#include "infra/range.h"

#include <date/date.h>
#include <optional>
#include <regex>
#include <unordered_map>

namespace emap {

class SectorInventory;
class PollutantInventory;

struct SpatialPatternSource
{
    enum class Type
    {
        SpatialPatternCAMS,  // Tiff containing the spatial pattern
        SpatialPatternCEIP,  // Tiff containing the spatial pattern
        RasterException,     // Tiff containing the spatial pattern
        SpatialPatternTable, // Csv file containing information per cell
        UnfiformSpread,      // No data available, use a uniform spread
    };

    static SpatialPatternSource create_from_cams(const fs::path& path, const Country& country, const EmissionSector& sector, const Pollutant& pol, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPatternCAMS;
        source.path        = path;
        source.emissionId  = EmissionIdentifier(country, sector, pol);
        source.year        = year;
        source.sectorLevel = secLevel;
        return source;
    }

    static SpatialPatternSource create_from_ceip(const fs::path& path, const Country& country, const EmissionSector& sector, const Pollutant& pol, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPatternCEIP;
        source.path        = path;
        source.emissionId  = EmissionIdentifier(country, sector, pol);
        source.year        = year;
        source.sectorLevel = secLevel;
        return source;
    }

    static SpatialPatternSource create_from_table(const fs::path& path, const Country& country, const EmissionSector& sector, const Pollutant& pol, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPatternTable;
        source.path        = path;
        source.emissionId  = EmissionIdentifier(country, sector, pol);
        source.year        = year;
        source.sectorLevel = secLevel;
        return source;
    }

    static SpatialPatternSource create_with_uniform_spread(const Country& country, const EmissionSector& sector, const Pollutant& pol)
    {
        SpatialPatternSource source;
        source.type       = Type::UnfiformSpread;
        source.emissionId = EmissionIdentifier(country, sector, pol);
        return source;
    }

    Type type = Type::SpatialPatternCAMS;
    fs::path path;
    EmissionIdentifier emissionId;
    // These fields are only relevant when the type is SpatialPattern
    date::year year;
    EmissionSector::Type sectorLevel = EmissionSector::Type::Nfr;
};

class SpatialPatternInventory
{
public:
    SpatialPatternInventory(const SectorInventory& sectorInventory, const PollutantInventory& pollutantInventory, const CountryInventory& countryInventory, const fs::path& exceptionsFile);

    void scan_dir(date::year reportingYear, date::year startYear, const fs::path& spatialPatternPath);
    SpatialPatternSource get_spatial_pattern(const EmissionIdentifier& emissionId) const;

private:
    struct SpatialPatternFile
    {
        enum class Source
        {
            Cams,
            Ceip,
            SpreadSheet,
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
        inf::Range<date::year> yearRange;
        EmissionIdentifier emissionId;
        fs::path spatialPattern;
    };

    std::vector<SpatialPatternException> parse_spatial_pattern_exceptions(const fs::path& exceptionsFile) const;
    std::optional<SpatialPatternSource> search_spatial_pattern_within_year(const Country& country,
                                                                           const Pollutant& pol,
                                                                           const Pollutant& polToReport,
                                                                           const EmissionSector& sector,
                                                                           date::year year,
                                                                           const std::vector<SpatialPatternFile>& patterns) const;

    std::optional<SpatialPatternFile> identify_spatial_pattern_cams(const fs::path& path) const;
    std::optional<SpatialPatternFile> identify_spatial_pattern_ceip(const fs::path& path) const;
    std::optional<SpatialPatternFile> identify_spatial_pattern_belgium(const fs::path& path) const;
    std::vector<SpatialPatterns> scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const;
    std::vector<SpatialPatterns> scan_dir_belgium(date::year startYear, const fs::path& spatialPatternPath) const;

    std::optional<SpatialPatternException> find_exception(const EmissionIdentifier& emissionId) const noexcept;

    fs::path _exceptionsFile;
    const SectorInventory& _sectorInventory;
    const PollutantInventory& _pollutantInventory;
    const CountryInventory& _countryInventory;
    std::regex _spatialPatternCamsRegex;
    std::regex _spatialPatternCeipRegex;
    std::regex _spatialPatternBelgium1Regex;
    std::regex _spatialPatternBelgium2Regex;
    // Contains all the exceptions for the configured year
    std::vector<SpatialPatternException> _exceptions;
    // Contains all the available patterns, sorted by year of preference
    std::vector<SpatialPatterns> _spatialPatternsRest;
    std::unordered_map<Country, std::vector<SpatialPatterns>> _countrySpecificSpatialPatterns;
};

}