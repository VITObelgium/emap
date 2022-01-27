#pragma once

#include "emap/country.h"
#include "emap/griddefinition.h"
#include "emap/sector.h"
#include "infra/filesystem.h"
//#include "infra/generator.h"
#include "infra/geometadata.h"
#include "infra/progressinfo.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <gdx/rasterfwd.h>

namespace geos::geom {
class Geometry;
}

namespace emap {

class CountryInventory;

using namespace inf;

struct GridProcessingProgressInfo
{
    GridProcessingProgressInfo(Country country_)
    : country(country_)
    {
    }

    Country country;
};

struct CellCoverageInfo
{
    constexpr CellCoverageInfo() noexcept;
    CellCoverageInfo(Cell c, double cov) noexcept;

    constexpr bool operator==(const CellCoverageInfo& other) const noexcept
    {
        return cell == other.cell && coverage == other.coverage;
    }

    Cell cell;
    double coverage = 0.0;
};

using GridProcessingProgress = inf::ProgressTracker<Country>;
using CountryCellCoverage    = std::pair<Country, std::vector<CellCoverageInfo>>;

// normalizes the raster so the sum is 1
void normalize_raster(gdx::DenseRaster<double>& ras) noexcept;

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid);
gdx::DenseRaster<double> read_raster_north_up(const fs::path& rasterInput);

gdx::DenseRaster<double> spread_values_uniformly_over_cells(double valueToSpread, GridDefinition grid, std::span<const CellCoverageInfo> cellCoverages);

size_t known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField);
std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb);

void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const std::string& countryIdField, const fs::path& outputDir, std::string_view filenameFormat, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb);
void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb);

// cuts out the country from the raster based on the cellcoverages, the output extent will be the same is that from the input
// The resulting raster is normalized
gdx::DenseRaster<double> extract_country_from_raster(const gdx::DenseRaster<double>& rasterInput, std::span<const CellCoverageInfo> cellCoverages);
gdx::DenseRaster<double> extract_country_from_raster(const fs::path& rasterInput, std::span<const CellCoverageInfo> cellCoverages);

// generator<std::pair<gdx::DenseRaster<double>, Country>> extract_countries_from_raster(const fs::path& rasterInput, GnfrSector gnfrSector, std::span<const CountryCellCoverage> countries);

}

namespace fmt {
template <>
struct formatter<emap::CellCoverageInfo>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::CellCoverageInfo& cov, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{} covers {}%", cov.cell, cov.coverage);
    }
};
}
