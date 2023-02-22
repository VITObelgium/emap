#pragma once

#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/modelpaths.h"
#include "emap/sectorinventory.h"
#include "infra/filesystem.h"

#include <date/date.h>
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
    };

    RunConfiguration(
        const fs::path& dataPath,
        const fs::path& spatialPatternExceptions,
        ModelGrid grid,
        ValidationType validation,
        date::year year,
        date::year reportYear,
        std::string_view scenario,
        std::vector<Pollutant> includedPollutants,
        SectorInventory sectors,
        PollutantInventory pollutants,
        CountryInventory countries,
        Output outputConfig);

    fs::path point_source_emissions_dir_path(const Country& country) const;
    fs::path total_emissions_path_nfr(date::year year, date::year reportYear) const;
    fs::path total_extra_emissions_path_nfr() const;
    fs::path total_emissions_path_gnfr(date::year reportYear) const;
    fs::path total_emissions_path_nfr_belgium(const Country& belgianRegian) const;
    fs::path spatial_pattern_path() const;

    fs::path emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const;
    fs::path emission_brn_output_path(date::year year, const Pollutant& pol, const EmissionSector& sector) const;

    fs::path scalings_path() const;

    const fs::path& data_root() const noexcept;
    const fs::path& output_path() const noexcept;
    const fs::path& spatial_pattern_exceptions() const noexcept;
    fs::path boundaries_vector_path() const noexcept;
    fs::path eez_boundaries_vector_path() const noexcept;
    std::string boundaries_field_id() const noexcept;
    std::string eez_boundaries_field_id() const noexcept;

    ModelGrid model_grid() const noexcept;
    ValidationType validation_type() const noexcept;

    date::year year() const noexcept;
    void set_year(date::year year) noexcept;

    date::year reporting_year() const noexcept;

    std::string_view scenario() const noexcept;

    void set_max_concurrency(std::optional<int32_t> concurrency) noexcept;
    std::optional<int32_t> max_concurrency() const noexcept;

    std::vector<Pollutant> included_pollutants() const;

    const SectorInventory& sectors() const noexcept;
    const PollutantInventory& pollutants() const noexcept;
    const CountryInventory& countries() const noexcept;

    SectorLevel output_sector_level() const noexcept;
    std::string_view output_sector_level_name() const noexcept;
    std::string_view output_filename_suffix() const noexcept;
    bool output_country_rasters() const noexcept;
    bool output_grid_rasters() const noexcept;
    bool output_spatial_pattern_rasters() const noexcept;
    fs::path output_dir_for_rasters() const;
    fs::path output_path_for_country_raster(const EmissionIdentifier& id, const GridData& grid) const;
    fs::path output_path_for_grid_raster(const Pollutant& pol, const EmissionSector& sector, const GridData& grid) const;
    fs::path output_path_for_spatial_pattern_raster(const EmissionIdentifier& id, const GridData& grid) const;

private:
    ModelPaths _paths;
    fs::path _spatialPatternExceptions;
    ModelGrid _grid;
    ValidationType _validation;
    date::year _year;
    date::year _reportYear;
    std::string _scenario;
    std::vector<Pollutant> _includedPollutants;
    SectorInventory _sectorInventory;
    PollutantInventory _pollutantInventory;
    CountryInventory _countryInventory;

    std::optional<int32_t> _concurrency;

    Output _outputConfig;
};

}
