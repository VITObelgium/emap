#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <optional>
#include <regex>
#include <unordered_map>

namespace emap {

struct SpatialPatternSource
{
    enum class Type
    {
        SpatialPatternRaster, // Tiff containing the spatial pattern
        SpatialPatternTable,  // Excel file containing information per cell
        UnfiformSpread,       // No data available, use a uniform spread
    };

    static SpatialPatternSource create_from_raster(const fs::path& path, Pollutant pol, EmissionSector sector, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPatternRaster;
        source.path        = path;
        source.pollutant   = pol;
        source.sector      = sector;
        source.year        = year;
        source.sectorLevel = secLevel;
        return source;
    }

    static SpatialPatternSource create_from_table(const fs::path& path, Pollutant pol, EmissionSector sector, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPatternTable;
        source.path        = path;
        source.pollutant   = pol;
        source.sector      = sector;
        source.year        = year;
        source.sectorLevel = secLevel;
        return source;
    }

    static SpatialPatternSource create_with_uniform_spread(Pollutant pol, EmissionSector sec)
    {
        SpatialPatternSource source;
        source.type      = Type::UnfiformSpread;
        source.sector    = sec;
        source.pollutant = pol;
        return source;
    }

    Type type = Type::SpatialPatternRaster;
    fs::path path;
    Pollutant pollutant;
    EmissionSector sector;
    // These fields are only relevant when the type is SpatialPattern
    date::year year;
    EmissionSector::Type sectorLevel = EmissionSector::Type::Nfr;
};

class SpatialPatternInventory
{
public:
    SpatialPatternInventory();

    void scan_dir(date::year reportingYear, date::year startYear, const fs::path& spatialPatternPath);
    SpatialPatternSource get_spatial_pattern(Country country, Pollutant pol, EmissionSector sector) const;

private:
    struct SpatialPatternFile
    {
        fs::path path;
        Pollutant pollutant;
        EmissionSector sector;
    };

    struct SpatialPatterns
    {
        date::year year;
        std::vector<SpatialPatternFile> spatialPatterns;
    };

    std::optional<SpatialPatternFile> identify_spatial_pattern_cams(const fs::path& path) const;
    std::optional<SpatialPatternFile> identify_spatial_pattern_excel(const fs::path& path) const;
    std::vector<SpatialPatterns> scan_dir_rest(date::year startYear, const fs::path& spatialPatternPath) const;
    std::vector<SpatialPatterns> scan_dir_belgium(date::year startYear, const fs::path& spatialPatternPath) const;

    std::regex _spatialPatternCamsRegex;
    std::regex _spatialPatternExcelRegex;
    // Contains all the available patterns, sorted by year of preference
    std::vector<SpatialPatterns> _spatialPatternsRest;
    std::unordered_map<Country, std::vector<SpatialPatterns>> _countrySpecificSpatialPatterns;
};

}