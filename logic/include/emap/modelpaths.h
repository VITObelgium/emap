#pragma once

#include "emap/country.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/pollutant.h"
#include "infra/filesystem.h"

#include <date/date.h>

namespace emap {

class ModelPaths
{
public:
    ModelPaths(std::string_view scenario, const fs::path& dataRoot, const fs::path& outputRoot);

    fs::path point_source_emissions_dir_path(const Country& country, date::year reportYear) const;
    fs::path total_emissions_path_nfr(date::year year, date::year reportYear, date::year lookupReportYear) const;
    fs::path total_extra_emissions_path_nfr(date::year reportYear) const;
    fs::path total_emissions_path_gnfr(date::year reportYear, date::year lookupReportYear) const;
    fs::path total_emissions_path_nfr_belgium(const Country& belgianRegian, date::year reportYear) const;
    fs::path spatial_pattern_path() const;

    fs::path sector_parameters_config_path() const;

    fs::path emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const;
    fs::path emission_brn_output_path(date::year year, const Pollutant& pol, const EmissionSector& sector) const;

    const fs::path& data_root() const noexcept;
    void set_data_root(const fs::path& root);
    const fs::path& output_path() const noexcept;
    fs::path boundaries_vector_path() const noexcept;
    fs::path eez_boundaries_vector_path() const noexcept;

    fs::path output_dir_for_rasters() const;
    fs::path output_path_for_country_raster(const EmissionIdentifier& id, const GridData& grid) const;
    fs::path output_path_for_grid_raster(const Pollutant& pol, const EmissionSector& sector, const GridData& grid) const;
    fs::path output_path_for_spatial_pattern_raster(const EmissionIdentifier& id, const GridData& grid) const;

private:
    fs::path emissions_dir_path(date::year reportYear) const;
    fs::path append_scenario_suffix_if_available(const fs::path& path) const;

    std::string _scenario;
    fs::path _dataRoot;
    fs::path _outputRoot;
};

}
