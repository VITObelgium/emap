#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <optional>
#include <regex>

namespace emap {

struct SpatialPatternSource
{
    enum class Type
    {
        SpatialPattern,
        UnfiformSpread,
    };

    static SpatialPatternSource create(const fs::path& path, Pollutant pol, EmissionSector sector, date::year year, EmissionSector::Type secLevel)
    {
        SpatialPatternSource source;
        source.type        = Type::SpatialPattern;
        source.path        = path;
        source.sector      = sector;
        source.pollutant   = pol;
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

    Type type = Type::SpatialPattern;
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
    SpatialPatternSource get_spatial_pattern(Pollutant pol, EmissionSector sector) const;

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

    std::optional<SpatialPatternFile> identify_spatial_pattern_file(const fs::path& path) const;

    std::regex _spatialPatternRegex;
    // Contains all the available patterns, sorted by year of preference
    std::vector<SpatialPatterns> _spatialPatterns;
};

}