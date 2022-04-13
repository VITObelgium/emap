#pragma once

#include "emap/country.h"
#include "emap/griddefinition.h"
#include "emap/sector.h"
#include "infra/filesystem.h"

#include "infra/gdalalgo.h"

#include "infra/cell.h"
#include "infra/gdal.h"
#include "infra/geometadata.h"
#include "infra/progressinfo.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <gdx/rasterfwd.h>

namespace geos::geom {
class Geometry;
}

namespace inf::gdal {
class SpatialReference;
}

namespace emap {

class CountryInventory;

struct GridProcessingProgressInfo
{
    GridProcessingProgressInfo(Country country_)
    : country(country_)
    {
    }

    Country country;
};

using GridProcessingProgress = inf::ProgressTracker<Country>;

struct CountryCellCoverage
{
    struct CellInfo
    {
        CellInfo() noexcept = default;
        CellInfo(inf::Cell compute, inf::Cell country, double cov)
        : computeGridCell(compute)
        , countryGridCell(country)
        , coverage(cov)
        {
        }

        inf::Cell computeGridCell; // row column index of this cell in the full output grid
        inf::Cell countryGridCell; // row column index of this cell in the country sub grid of the spatial pattern grid
        double coverage = 0.0;     // The cell coverage percentage of this country in the grid

        constexpr bool operator==(const CellInfo& other) const noexcept
        {
            // Don't compare the country cell, we want to compare cells from different countries in the compute grid
            return computeGridCell == other.computeGridCell && coverage == other.coverage;
        }
    };

    Country country;
    inf::GeoMetadata outputSubgridExtent; // This countries subgrid within the output grid, depending on the coverageMode this is contained in the output grid or not
    std::vector<CellInfo> cells;
};

// normalizes the raster so the sum is 1
void normalize_raster(gdx::DenseRaster<double>& ras) noexcept;

inf::gdal::VectorDataSet transform_vector(const fs::path& vectorPath, const inf::GeoMetadata& destMeta);

gdx::DenseRaster<double> transform_grid(const gdx::DenseRaster<double>& ras, GridDefinition grid, inf::gdal::ResampleAlgorithm algo = inf::gdal::ResampleAlgorithm::Average);
gdx::DenseRaster<double> read_raster_north_up(const fs::path& rasterInput, const inf::GeoMetadata& extent);

gdx::DenseRaster<double> spread_values_uniformly_over_cells(double valueToSpread, const CountryCellCoverage& countryCoverage);

inf::GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent);
inf::GeoMetadata create_geometry_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent, const inf::gdal::SpatialReference& sourceProjection);

inf::GeoMetadata create_geometry_intersection_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent);
inf::GeoMetadata create_geometry_intersection_extent(const geos::geom::Geometry& geom, const inf::GeoMetadata& gridExtent, const inf::gdal::SpatialReference& sourceProjection);

std::unordered_set<CountryId> known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, const fs::path& countriesVector, const std::string& countryIdField);
std::unordered_set<CountryId> known_countries_in_extent(const CountryInventory& inv, const inf::GeoMetadata& extent, inf::gdal::VectorDataSet& countriesDs, const std::string& countryIdField);

enum class CoverageMode
{
    GridCellsOnly,
    AllCountryCells,
};

CountryCellCoverage create_country_coverage(const Country& country, const geos::geom::Geometry& geom, const inf::gdal::SpatialReference& geometryProjection, const inf::GeoMetadata& outputExtent, CoverageMode mode);
std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent, const fs::path& countriesVector, const std::string& countryIdField, const CountryInventory& inv, CoverageMode mode, const GridProcessingProgress::Callback& progressCb);
std::vector<CountryCellCoverage> create_country_coverages(const inf::GeoMetadata& outputExtent, inf::gdal::VectorDataSet& countriesDs, const std::string& countryIdField, const CountryInventory& inv, CoverageMode mode, const GridProcessingProgress::Callback& progressCb);

// void extract_countries_from_raster(const fs::path& rasterInput, const fs::path& countriesShape, const std::string& countryIdField, const fs::path& outputDir, std::string_view filenameFormat, const CountryInventory& inv, const GridProcessingProgress::Callback& progressCb);
// void extract_countries_from_raster(const fs::path& rasterInput, std::span<const CountryCellCoverage> countries, const fs::path& outputDir, std::string_view filenameFormat, const GridProcessingProgress::Callback& progressCb);

// cuts out the country from the raster based on the cellcoverages, the output extent will be the same as that from the input
gdx::DenseRaster<double> extract_country_from_raster(const gdx::DenseRaster<double>& rasterInput, const CountryCellCoverage& countryCoverage);
gdx::DenseRaster<double> extract_country_from_raster(const fs::path& rasterInput, const CountryCellCoverage& countryCoverage);

// generator<std::pair<gdx::DenseRaster<double>, Country>> extract_countries_from_raster(const fs::path& rasterInput, GnfrSector gnfrSector, std::span<const CountryCellCoverage> countries);

void erase_area_in_raster(gdx::DenseRaster<double>& rasterInput, const inf::GeoMetadata& extent);
double erase_area_in_raster_and_sum_erased_values(gdx::DenseRaster<double>& rasterInput, const inf::GeoMetadata& extent);

}

namespace fmt {
template <>
struct formatter<emap::CountryCellCoverage::CellInfo>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::CountryCellCoverage::CellInfo& info, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{} covers {}%", info.computeGridCell, info.coverage * 100);
    }
};
}
