#pragma once

#include "emap/emissions.h"
#include "gdx/denseraster.h"

#include <date/date.h>

namespace emap {

struct SpatialPatternData
{
    date::year year;
    EmissionIdentifier id;
    gdx::DenseRaster<double> raster;
};

struct SpatialPatternSource
{
    enum class Type
    {
        SpatialPatternCAMS,     // Tiff containing the spatial pattern
        SpatialPatternCEIP,     // Txt file containing the spatial pattern
        SpatialPatternFlanders, // Csv file containing information per cell
        Raster,                 // Tiff containing the spatial pattern
        UnfiformSpread,         // No data available, use a uniform spread
    };

    static SpatialPatternSource create_from_cams(const fs::path& path,
                                                 const EmissionIdentifier& id,
                                                 const EmissionIdentifier& usedId,
                                                 date::year year,
                                                 bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternCAMS;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_from_ceip(const fs::path& path,
                                                 const EmissionIdentifier& id,
                                                 const EmissionIdentifier& usedId,
                                                 date::year year,
                                                 bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternCEIP;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
        source.isException    = exception;
        return source;
    }

    static SpatialPatternSource create_from_flanders(const fs::path& path,
                                                     const EmissionIdentifier& id,
                                                     const EmissionIdentifier& usedId,
                                                     date::year year,
                                                     bool exception)
    {
        SpatialPatternSource source;
        source.type           = Type::SpatialPatternFlanders;
        source.path           = path;
        source.emissionId     = id;
        source.usedEmissionId = usedId;
        source.year           = year;
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
    std::optional<date::year> year;
};

struct SpatialPattern
{
    SpatialPattern() noexcept = default;
    explicit SpatialPattern(const SpatialPatternSource& src)
    : source(src)
    {
    }

    SpatialPatternSource source;
    gdx::DenseRaster<double> raster;
};

}