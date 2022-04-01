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

struct SpatialPatternSource
{
    enum class Type
    {
        SpatialPatternCAMS,  // Tiff containing the spatial pattern
        SpatialPatternCEIP,  // Txt file containing the spatial pattern
        SpatialPatternTable, // Csv file containing information per cell
        Raster,              // Tiff containing the spatial pattern
        UnfiformSpread,      // No data available, use a uniform spread
    };

    static SpatialPatternSource create_from_cams(const fs::path& path,
                                                 const EmissionIdentifier& id,
                                                 const EmissionIdentifier& usedId,
                                                 date::year year,
                                                 EmissionSector::Type secLevel,
                                                 bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternCAMS;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
        source.sectorLevel    = secLevel;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_from_ceip(const fs::path& path,
                                                 const EmissionIdentifier& id,
                                                 const EmissionIdentifier& usedId,
                                                 date::year year,
                                                 EmissionSector::Type secLevel,
                                                 bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternCEIP;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
        source.sectorLevel    = secLevel;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_from_table(const fs::path& path,
                                                  const EmissionIdentifier& id,
                                                  const EmissionIdentifier& usedId,
                                                  date::year year,
                                                  EmissionSector::Type secLevel,
                                                  bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternTable;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
        source.sectorLevel    = secLevel;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_from_raster(const fs::path& path,
                                                   const EmissionIdentifier& id,
                                                   const EmissionIdentifier& usedId,
                                                   bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::Raster;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_with_uniform_spread(const Country& country,
                                                           const EmissionSector& sector,
                                                           const Pollutant& pol,
                                                           bool dueToMissingData)
    {
        SpatialPatternSource source;
        source.type                           = Type::UnfiformSpread;
        source.emissionId                     = EmissionIdentifier(country, sector, pol);
        source.usedEmissionId                 = source.emissionId;
        source.patternAvailableButWithoutData = dueToMissingData;
        return source;
    }

    Type type                           = Type::SpatialPatternCAMS;
    bool patternAvailableButWithoutData = false;
    bool isException                    = false; // Is used because it was configured in the exceptions file
    fs::path path;
    EmissionIdentifier emissionId;
    EmissionIdentifier usedEmissionId; // the actual emissionidentifer used to lookup the spatial pattern (can be different because of pollutant fallbacks or via sector overrides)
    // These fields are only relevant when the type is SpatialPattern
    date::year year;
    EmissionSector::Type sectorLevel = EmissionSector::Type::Nfr;
};

class SpatialPatternTableCache
{
public:
    SpatialPatternTableCache(const RunConfiguration& cfg) noexcept;

    const SpatialPatternData* get_data(const fs::path& path, const EmissionIdentifier& id);

private:
    const SpatialPatternData* find_data_for_id(const std::vector<SpatialPatternData>& list, const EmissionIdentifier& emissionId) const noexcept;

    std::mutex _mutex;
    const RunConfiguration& _cfg;
    std::map<fs::path, std::unique_ptr<std::vector<SpatialPatternData>>> _patterns;
};

class SpatialPatternInventory
{
public:
    SpatialPatternInventory(const RunConfiguration& cfg);

    void scan_dir(date::year reportingYear, date::year startYear, const fs::path& spatialPatternPath);
    SpatialPatternSource get_spatial_pattern(const EmissionIdentifier& emissionId, SpatialPatternTableCache* cache = nullptr) const;

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
    std::optional<SpatialPatternFile> identify_spatial_pattern_belgium(const fs::path& path) const;
    std::vector<SpatialPatterns> scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const;
    std::vector<SpatialPatterns> scan_dir_belgium(date::year startYear, const fs::path& spatialPatternPath) const;

    SpatialPatternSource get_country_specific_spatial_pattern(EmissionIdentifier emissionId, const std::vector<SpatialPatterns>& patterns, const EmissionSector& sectorToReport, SpatialPatternTableCache* cache) const;
    SpatialPatternSource get_spatial_pattern(const EmissionIdentifier& emissionId, const std::vector<SpatialPatterns>& patterns, const EmissionSector& sectorToReport) const;

    std::optional<SpatialPatternException> find_exception(const EmissionIdentifier& emissionId) const noexcept;
    static SpatialPatternSource source_from_exception(const SpatialPatternException& ex, const Pollutant& pollutantToReport, const EmissionSector& emissionSectorToReport, date::year year);
    static SpatialPatternException::Type exception_type_from_string(std::string_view str);

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
};

}