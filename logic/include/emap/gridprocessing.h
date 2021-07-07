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
    GridProcessingProgressInfo(Country::Id country_, int64_t currentCell_, int64_t cellCount_)
    : country(country_)
    , currentCell(currentCell_)
    , cellCount(cellCount_)
    {
    }

    Country::Id country = Country::Id::Invalid;
    int64_t currentCell = 0;
    int64_t cellCount   = 0;
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

using GridProcessingProgress = inf::ProgressTracker<GridProcessingProgressInfo>;
using CountryCellCoverage    = std::pair<Country::Id, std::vector<CellCoverageInfo>>;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid);

std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField);
void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const fs::path& outputDir);
void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, GridProcessingProgress::Callback progressCb = nullptr);

}
