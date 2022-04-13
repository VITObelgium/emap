#include "emap/modelpaths.h"

#include <fmt/format.h>

namespace emap {
ModelPaths::ModelPaths(const fs::path& dataRoot, const fs::path& outputRoot)
: _dataRoot(dataRoot)
, _outputRoot(outputRoot)
{
}

fs::path ModelPaths::point_source_emissions_dir_path(const Country& country, date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "pointsources" / country.iso_code();
}

fs::path ModelPaths::total_emissions_path_nfr(date::year year, date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "totals" / fmt::format("nfr_{}_{}.txt", static_cast<int>(year), static_cast<int>(reportYear));
}

fs::path ModelPaths::total_extra_emissions_path_nfr(date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "totals" / fmt::format("nfr_allyears_{}_extra.txt", static_cast<int>(reportYear));
}

fs::path ModelPaths::total_emissions_path_gnfr(date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "totals" / fmt::format("gnfr_allyears_{}.txt", static_cast<int>(reportYear));
}

fs::path ModelPaths::total_emissions_path_nfr_belgium(const Country& belgianRegian, date::year reportYear) const
{
    if (!belgianRegian.is_belgium()) {
        throw std::logic_error("Internal error: a belgian region is required");
    }

    return emissions_dir_path(reportYear) / "totals" / fmt::format("{}_{}.xlsx", belgianRegian.iso_code(), static_cast<int>(reportYear));
}

fs::path ModelPaths::spatial_pattern_path() const
{
    return _dataRoot / "03_spatial_disaggregation";
}

fs::path ModelPaths::sector_parameters_config_path() const
{
    return _dataRoot / "05_model_parameters" / "sector_parameters.xlsx";
}

fs::path ModelPaths::emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const
{
    return output_path() / std::to_string(static_cast<int>(year)) / fs::u8path(fmt::format("{}_{}_{}.tif", emissionId.pollutant.code(), emissionId.sector.name(), emissionId.country.iso_code()));
}

fs::path ModelPaths::emission_brn_output_path(date::year year, const Pollutant& pol, const EmissionSector& sector) const
{
    const int yearInt = static_cast<int>(year);
    return output_path() / std::to_string(yearInt) / fs::u8path(fmt::format("{}_{}_{}.brn", pol.code(), sector, yearInt));
}

fs::path ModelPaths::diffuse_scalings_path(date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "scaling_diffuse.csv";
}

fs::path ModelPaths::point_source_scalings_path(date::year reportYear) const
{
    return emissions_dir_path(reportYear) / "scaling_pointsources.csv";
}

const fs::path& ModelPaths::data_root() const noexcept
{
    return _dataRoot;
}

const fs::path& ModelPaths::output_path() const noexcept
{
    return _outputRoot;
}

fs::path ModelPaths::boundaries_vector_path() const noexcept
{
    return _dataRoot / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";
}

fs::path ModelPaths::eez_boundaries_vector_path() const noexcept
{
    return _dataRoot / "03_spatial_disaggregation" / "boundaries" / "boundaries_incl_EEZ.gpkg";
}

fs::path ModelPaths::output_dir_for_rasters() const
{
    return output_path() / "rasters";
}

fs::path ModelPaths::output_path_for_country_raster(const EmissionIdentifier& id, const GridData& grid) const
{
    return output_dir_for_rasters() / fs::u8path(fmt::format("{}_{}_{}_{}.tif", id.country.iso_code(), id.pollutant.code(), id.sector.name(), grid.name));
}

fs::path ModelPaths::output_path_for_grid_raster(const Pollutant& pol, const EmissionSector& sector, const GridData& grid) const
{
    return output_dir_for_rasters() / fs::u8path(fmt::format("{}_{}_{}.tif", pol.code(), sector.name(), grid.name));
}

fs::path ModelPaths::emissions_dir_path(date::year reportYear) const
{
    return _dataRoot / "01_data_emissions" / "inventory" / fs::u8path(fmt::format("reporting_{}", static_cast<int>(reportYear)));
}

}
