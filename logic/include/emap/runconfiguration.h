#pragma once

#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/modelpaths.h"
#include "emap/sectorinventory.h"
#include "infra/filesystem.h"
#include "infra/chrono.h"

#include <fmt/core.h>
#include <optional>

namespace emap {

enum class SectorLevel
{
    GNFR,
    NFR,
    Custom,
};

enum class ValidationType
{
    NoValidation,
    SumValidation,
};

class RunConfiguration
{
public:
    struct Output
    {
        fs::path path;
        std::string filenameSuffix; // optional output filename suffix;
        std::string outputLevelName;
        bool createCountryRasters        = false;
        bool createGridRasters           = false;
        bool createSpatialPatternRasters = false;
        bool separatePointSources        = false;
    };

    RunConfiguration(
        const fs::path& dataPath,
        const fs::path& spatialPatternExceptions,
        const fs::path& emissionScalings,
        const fs::path& spatialBoundariesFilename,
        const fs::path& spatialBoundariesEezFilename,
        ModelGrid grid,
        ValidationType validation,
        inf::chrono::year year,
        inf::chrono::year reportYear,
        std::string_view scenario,
        bool combineIdenticalPointSources,
        double rescaleThreshold,
        std::vector<Pollutant> includedPollutants,
        SectorInventory sectors,
        PollutantInventory pollutants,
        CountryInventory countries,
        Output outputConfig,
        std::optional<GridData> configuredGrid = std::nullopt);

    fs::path point_source_emissions_dir_path(const Country& country) const;
    fs::path total_emissions_path_nfr(inf::chrono::year year, inf::chrono::year reportYear) const;
    fs::path total_extra_emissions_path_nfr() const;
    fs::path total_emissions_path_gnfr(inf::chrono::year reportYear) const;
    fs::path total_emissions_path_nfr_belgium(const Country& belgianRegian) const;
    fs::path spatial_pattern_path() const;

    fs::path emission_output_raster_path(inf::chrono::year year, const EmissionIdentifier& emissionId) const;
    fs::path emission_brn_output_path(inf::chrono::year year, const Pollutant& pol, const EmissionSector& sector) const;

    bool pmcoarse_calculation_needed() const noexcept;

    const fs::path& data_root() const noexcept;
    void set_data_root(const fs::path& root);

    const fs::path& output_path() const noexcept;
    const fs::path& spatial_pattern_exceptions() const noexcept;
    const fs::path& emission_scalings_path() const noexcept;
    fs::path boundaries_vector_path() const noexcept;
    fs::path eez_boundaries_vector_path() const noexcept;
    std::string boundaries_field_id() const noexcept;
    std::string eez_boundaries_field_id() const noexcept;

    ModelGrid model_grid() const noexcept;
    const GridData& grid_data(GridDefinition grid) const;
    ModelOuputFormat model_output_format() const;
    ValidationType validation_type() const noexcept;

    inf::chrono::year year() const noexcept;
    void set_year(inf::chrono::year year) noexcept;

    inf::chrono::year reporting_year() const noexcept;

    std::string_view scenario() const noexcept;

    bool combine_identical_point_sources() const noexcept;
    void set_combine_identical_point_sources(bool enabled) noexcept;

    double point_source_rescale_threshold() const noexcept;

    void set_max_concurrency(std::optional<int32_t> concurrency) noexcept;
    std::optional<int32_t> max_concurrency() const noexcept;

    std::vector<Pollutant> included_pollutants() const;
    bool pollutant_is_included(std::string_view pollutant) const noexcept;

    const SectorInventory& sectors() const noexcept;
    const PollutantInventory& pollutants() const noexcept;
    const CountryInventory& countries() const noexcept;

    SectorLevel output_sector_level() const noexcept;
    std::string_view output_sector_level_name() const noexcept;
    std::string_view output_filename_suffix() const noexcept;
    bool output_country_rasters() const noexcept;
    bool output_grid_rasters() const noexcept;
    bool output_spatial_pattern_rasters() const noexcept;
    bool output_point_sources_separately() const noexcept;
    fs::path output_dir_for_rasters() const;
    fs::path output_path_for_country_raster(const EmissionIdentifier& id, const GridData& grid) const;
    fs::path output_path_for_grid_raster(const Pollutant& pol, const EmissionSector& sector, const GridData& grid) const;
    fs::path output_path_for_spatial_pattern_raster(const EmissionIdentifier& id, const GridData& grid) const;

    fs::path sector_parameters_config_path() const noexcept;

private:
    ModelPaths _paths;
    fs::path _spatialPatternExceptions;
    fs::path _emissionScalingsPath;
    ModelGrid _grid;
    std::optional<GridData> _configuredGrid;
    ValidationType _validation;
    inf::chrono::year _year;
    inf::chrono::year _reportYear;
    std::string _scenario;
    bool _combineIdenticalPointSources;
    double _pointRescaleThreshold;
    std::vector<Pollutant> _includedPollutants;
    SectorInventory _sectorInventory;
    PollutantInventory _pollutantInventory;
    CountryInventory _countryInventory;

    std::optional<int32_t> _concurrency;

    Output _outputConfig;
};

}
