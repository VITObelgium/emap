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
    const fs::path& countriesPath,
    GridDefinition grid,
    RunType runType,
    ValidationType validation,
    date::year year,
    date::year reportYear,
    std::string_view scenario,
    SectorInventory sectors,
    PollutantInventory pollutants,
    CountryInventory countries,
    const fs::path& outputPath)
: _dataPath(dataPath)
, _countriesVectorPath(countriesPath)
, _outputPath(outputPath)
, _grid(grid)
, _runType(runType)
, _validation(validation)
, _year(year)
, _reportYear(reportYear)
, _scenario(scenario)
, _sectorInventory(std::move(sectors))
, _pollutantInventory(std::move(pollutants))
, _countryInventory(std::move(countries))
{
}

fs::path RunConfiguration::emissions_dir_path() const
{
    return _dataPath / "01_data_emissions" / "inventory" / fs::u8path(fmt::format("reporting_{}", static_cast<int>(_reportYear)));
}

fs::path RunConfiguration::point_source_emissions_path(const Country& country, const Pollutant& pol) const
{
    return emissions_dir_path() / "pointsources" / country.iso_code() / fmt::format("emap_{}_{}_{}_aangevuld.csv", pol.code(), static_cast<int>(_year), static_cast<int>(_reportYear));
}

fs::path RunConfiguration::total_emissions_path_nfr() const
{
    return emissions_dir_path() / "totals" / fmt::format("nfr_{}_{}.txt", static_cast<int>(_year), static_cast<int>(_reportYear));
}

fs::path RunConfiguration::total_emissions_path_gnfr() const
{
    return emissions_dir_path() / "totals" / fmt::format("gnfr_allyears_{}.txt", static_cast<int>(_reportYear));
}

fs::path RunConfiguration::spatial_pattern_path() const
{
    return _dataPath / "03_spatial_disaggregation";
}

fs::path RunConfiguration::emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const
{
    return output_path() / std::to_string(static_cast<int>(year)) / fs::u8path(fmt::format("{}_{}_{}.tif", emissionId.pollutant.code(), emissionId.sector.name(), emissionId.country.iso_code()));
}

fs::path RunConfiguration::emission_brn_output_path(date::year year, Pollutant pol, EmissionSector sector) const
{
    const int yearInt = static_cast<int>(year);
    return output_path() / std::to_string(yearInt) / fs::u8path(fmt::format("{}_{}_{}.brn", pol.code(), sector, yearInt));
}

fs::path RunConfiguration::diffuse_scalings_path() const
{
    return emissions_dir_path() / "scaling_diffuse.csv";
}

fs::path RunConfiguration::point_source_scalings_path() const
{
    return emissions_dir_path() / "scaling_pointsources.csv";
}

const fs::path& RunConfiguration::data_root() const noexcept
{
    return _dataPath;
}

const fs::path& RunConfiguration::output_path() const noexcept
{
    return _outputPath;
}

const fs::path& RunConfiguration::countries_vector_path() const noexcept
{
    return _countriesVectorPath;
}

fs::path RunConfiguration::run_summary_path() const
{
    return output_path() / "summary.txt";
}

GridDefinition RunConfiguration::grid_definition() const noexcept
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

date::year RunConfiguration::reporting_year() const noexcept
{
    return _reportYear;
}

std::string_view RunConfiguration::scenario() const noexcept
{
    return _scenario;
}

void RunConfiguration::set_max_concurrency(int32_t concurrency) noexcept
{
    _concurrency = concurrency;
}

std::optional<int32_t> RunConfiguration::max_concurrency() const noexcept
{
    return _concurrency;
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

}
