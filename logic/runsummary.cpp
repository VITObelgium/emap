#include "runsummary.h"

#include "enuminfo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>
#include <tabulate/table.hpp>

namespace emap {

using namespace inf;

void RunSummary::add_spatial_pattern_source(const SpatialPatternSource& source)
{
    _spatialPatterns.push_back(source);
}

void RunSummary::add_point_source(const fs::path& pointSource)
{
    _pointSources.push_back(pointSource);
}

void RunSummary::add_totals_source(const fs::path& totalsSource)
{
    _totalsSources.push_back(totalsSource);
}

static std::string spatial_pattern_source_type_to_string(SpatialPatternSource::Type type)
{
    switch (type) {
    case emap::SpatialPatternSource::Type::SpatialPatternRaster:
        return "Raster";
    case emap::SpatialPatternSource::Type::SpatialPatternTable:
        return "Excel";
    case emap::SpatialPatternSource::Type::UnfiformSpread:
        return "Uniform spread";
    }

    return "";
}

std::string RunSummary::spatial_pattern_usage_table() const
{
    using namespace tabulate;

    Table table;
    table.add_row({"Sector", "Pollutant", "Type", "Year", "Path"});

    for (const auto& sp : _spatialPatterns) {
        std::string sector(sp.sector.name());
        std::string pollutant(sp.pollutant.code());
        std::string type = spatial_pattern_source_type_to_string(sp.type);
        std::string year = sp.type == SpatialPatternSource::Type::UnfiformSpread ? "-" : std::to_string(static_cast<int>(sp.year));

        table.add_row({sector, pollutant, type, year, sp.path.filename().generic_u8string()});
    }

    std::stringstream result;
    result << table;
    return result.str();
}

std::string RunSummary::emission_source_usage_table() const
{
    using namespace tabulate;

    Table table;
    table.add_row({"Emission type", "Path"});

    for (const auto& ps : _pointSources) {
        table.add_row({"Point source", ps.u8string()});
    }

    for (const auto& ps : _totalsSources) {
        table.add_row({"Totals", ps.u8string()});
    }

    std::stringstream result;
    result << table;
    return result.str();
}

}
