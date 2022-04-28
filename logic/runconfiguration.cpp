#include "emap/runconfiguration.h"

#include "configurationutil.h"
#include "infra/exception.h"
#include "infra/string.h"

namespace emap {

using namespace inf;

std::string run_type_name(RunType type)
{
    switch (type) {
    case RunType::Emep:
        return "historic";
    case RunType::Gains:
        return "future";
    }

    throw RuntimeError("Invalid run type");
}

RunConfiguration::RunConfiguration(
    const fs::path& dataPath,
    const fs::path& spatialPatternExceptions,
    ModelGrid grid,
    RunType runType,
    ValidationType validation,
    date::year year,
    date::year reportYear,
    std::string_view scenario,
    std::vector<Pollutant> includedPollutants,
    SectorInventory sectors,
    PollutantInventory pollutants,
    CountryInventory countries,
    Output outputConfig)
: _paths(dataPath, outputConfig.path)
, _spatialPatternExceptions(spatialPatternExceptions)
, _grid(grid)
, _runType(runType)
, _validation(validation)
, _year(year)
, _reportYear(reportYear)
, _scenario(scenario)
, _includedPollutants(std::move(includedPollutants))
, _sectorInventory(std::move(sectors))
, _pollutantInventory(std::move(pollutants))
, _countryInventory(std::move(countries))
, _outputConfig(outputConfig)
{
}

fs::path RunConfiguration::point_source_emissions_dir_path(const Country& country) const
{
    return _paths.point_source_emissions_dir_path(country, _reportYear);
}

fs::path RunConfiguration::total_emissions_path_nfr(date::year year, date::year reportYear) const
{
    return _paths.total_emissions_path_nfr(year, _reportYear, reportYear);
}

fs::path RunConfiguration::total_extra_emissions_path_nfr() const
{
    return _paths.total_extra_emissions_path_nfr(_reportYear);
}

fs::path RunConfiguration::total_emissions_path_gnfr(date::year reportYear) const
{
    return _paths.total_emissions_path_gnfr(_reportYear, reportYear);
}

fs::path RunConfiguration::total_emissions_path_nfr_belgium(const Country& belgianRegian) const
{
    return _paths.total_emissions_path_nfr_belgium(belgianRegian, _reportYear);
}

fs::path RunConfiguration::spatial_pattern_path() const
{
    return _paths.spatial_pattern_path();
}

fs::path RunConfiguration::emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const
{
    return _paths.emission_output_raster_path(year, emissionId);
}

fs::path RunConfiguration::emission_brn_output_path(date::year year, const Pollutant& pol, const EmissionSector& sector) const
{
    return _paths.emission_brn_output_path(year, pol, sector);
}

fs::path RunConfiguration::diffuse_scalings_path() const
{
    return _paths.diffuse_scalings_path(_reportYear);
}

fs::path RunConfiguration::point_source_scalings_path() const
{
    return _paths.point_source_scalings_path(_reportYear);
}

const fs::path& RunConfiguration::data_root() const noexcept
{
    return _paths.data_root();
}

const fs::path& RunConfiguration::output_path() const noexcept
{
    return _paths.output_path();
}

const fs::path& RunConfiguration::spatial_pattern_exceptions() const noexcept
{
    return _spatialPatternExceptions;
}

fs::path RunConfiguration::boundaries_vector_path() const noexcept
{
    return _paths.boundaries_vector_path();
}

fs::path RunConfiguration::eez_boundaries_vector_path() const noexcept
{
    return _paths.eez_boundaries_vector_path();
}

std::string RunConfiguration::boundaries_field_id() const noexcept
{
    return "Code3";
}

std::string RunConfiguration::eez_boundaries_field_id() const noexcept
{
    return "ISO_SOV1";
}

ModelGrid RunConfiguration::model_grid() const noexcept
{
    return _grid;
}

RunType RunConfiguration::run_type() const noexcept
{
    return _runType;
}

ValidationType RunConfiguration::validation_type() const noexcept
{
    return _validation;
}

date::year RunConfiguration::year() const noexcept
{
    return _year;
}

void RunConfiguration::set_year(date::year year) noexcept
{
    _year = year;
}

date::year RunConfiguration::reporting_year() const noexcept
{
    return _reportYear;
}

std::string_view RunConfiguration::scenario() const noexcept
{
    return _scenario;
}

void RunConfiguration::set_max_concurrency(std::optional<int32_t> concurrency) noexcept
{
    _concurrency = concurrency;
}

std::optional<int32_t> RunConfiguration::max_concurrency() const noexcept
{
    return _concurrency;
}

std::vector<Pollutant> RunConfiguration::included_pollutants() const
{
    if (_includedPollutants.empty()) {
        return container_as_vector(_pollutantInventory.list());
    }

    return _includedPollutants;
}

const SectorInventory& RunConfiguration::sectors() const noexcept
{
    return _sectorInventory;
}

const PollutantInventory& RunConfiguration::pollutants() const noexcept
{
    return _pollutantInventory;
}

const CountryInventory& RunConfiguration::countries() const noexcept
{
    return _countryInventory;
}

SectorLevel RunConfiguration::output_sector_level() const noexcept
{
    if (str::iequals(output_sector_level_name(), "GNFR")) {
        return SectorLevel::GNFR;
    }

    if (str::iequals(output_sector_level_name(), "NFR")) {
        return SectorLevel::NFR;
    }

    return SectorLevel::Custom;
}

std::string_view RunConfiguration::output_sector_level_name() const noexcept
{
    return _outputConfig.outputLevelName;
}

std::string_view RunConfiguration::output_filename_suffix() const noexcept
{
    return _outputConfig.filenameSuffix;
}

bool RunConfiguration::output_country_rasters() const noexcept
{
    return _outputConfig.createCountryRasters;
}

bool RunConfiguration::output_grid_rasters() const noexcept
{
    return _outputConfig.createGridRasters;
}

fs::path RunConfiguration::output_dir_for_rasters() const
{
    return _paths.output_dir_for_rasters();
}

fs::path RunConfiguration::output_path_for_country_raster(const EmissionIdentifier& id, const GridData& grid) const
{
    return _paths.output_path_for_country_raster(id, grid);
}

fs::path RunConfiguration::output_path_for_grid_raster(const Pollutant& pol, const EmissionSector& sector, const GridData& grid) const
{
    return _paths.output_path_for_grid_raster(pol, sector, grid);
}

}
