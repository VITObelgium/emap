#include "emap/countryborders.h"

#include "infra/gdalalgo.h"
#include "infra/gdalio.h"

namespace emap {

using namespace inf;
using namespace std::string_literals;

CountryBorders::CountryBorders(const fs::path& vectorPath, std::string_view countryIdField, const GeoMetadata& gridExtent, const CountryInventory& inv)
: _ds(gdal::warp_vector(vectorPath, gridExtent, {"-nlt"s, "PROMOTE_TO_MULTI"s}))
, _idField(countryIdField)
, _inv(inv)
{
}

size_t CountryBorders::known_countries_in_extent(const inf::GeoMetadata& extent)
{
    return emap::known_countries_in_extent(_inv, extent, _ds, _idField);
}

std::vector<CountryCellCoverage> CountryBorders::create_country_coverages(const inf::GeoMetadata& extent, CoverageMode mode, const GridProcessingProgress::Callback& progressCb)
{
    return emap::create_country_coverages(extent, _ds, _idField, _inv, mode, progressCb);
}

}
