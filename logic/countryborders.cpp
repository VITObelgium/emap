#include "emap/countryborders.h"

#include "infra/gdalalgo.h"

namespace emap {

using namespace inf;
using namespace std::string_literals;

CountryBorders::CountryBorders(const fs::path& vectorPath, std::string_view countryIdField, const GeoMetadata& gridExtent, const CountryInventory& inv)
: _ds(gdal::warp(vectorPath, gridExtent))
, _idField(countryIdField)
, _inv(inv)
{
}

size_t CountryBorders::known_countries_in_extent(const inf::GeoMetadata& extent)
{
    return emap::known_countries_in_extent(_inv, extent, _ds, _idField);
}

std::vector<CountryCellCoverage> CountryBorders::create_country_coverages(const inf::GeoMetadata& extent, const GridProcessingProgress::Callback& progressCb)
{
    return emap::create_country_coverages(extent, _ds, _idField, _inv, progressCb);
}

}
