#pragma once

#include "emap/country.h"
#include "emap/griddefinition.h"
#include "infra/filesystem.h"
#include "infra/geometadata.h"
#include "infra/progressinfo.h"
#include "infra/span.h"

#include <gdx/rasterfwd.h>

namespace emap {

using namespace inf;

struct GridProcessingProgressInfo
{
    GridProcessingProgressInfo(Country::Id country_)
    : country(country_)
    {
    }

    Country::Id country = Country::Id::Invalid;
};

struct CellCoverageInfo
{
    constexpr CellCoverageInfo() noexcept = default;
    constexpr CellCoverageInfo(Cell c, double cov) noexcept
    : cell(c)
    , coverage(cov)
    {
    }

    Cell cell;
    double coverage = 0.0;
};

using GridProcessingProgress = inf::ProgressTracker<Country::Id>;
using CountryCellCoverage    = std::pair<Country::Id, std::vector<CellCoverageInfo>>;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid);
gdx::DenseRaster<double> read_raster_north_up(const fs::path& rasterInput);

size_t known_countries_in_extent(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField);
std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField, const GridProcessingProgress::Callback& progressCb);
void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& outputDir);
void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, const GridProcessingProgress::Callback& progressCb);

}
