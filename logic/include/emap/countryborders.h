#pragma once

#include "emap/gridprocessing.h"
#include "infra/filesystem.h"
#include "infra/gdal.h"

#include <string>

namespace emap {

class CountryInventory;

class CountryBorders
{
public:
    CountryBorders(const fs::path& vectorPath, std::string_view countryIdField, const inf::GeoMetadata& gridExtent, const CountryInventory& inv);

    size_t known_countries_in_extent(const inf::GeoMetadata& extent);
    std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, CoverageMode mode, const GridProcessingProgress::Callback& progressCb);

private:
    inf::gdal::VectorDataSet _ds;
    std::string _idField;
    const CountryInventory& _inv;
};
}
