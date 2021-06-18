#include "emap/runconfiguration.h"

#include "infra/exception.h"

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
    GridDefinition grid,
    RunType runType,
    ValidationType validation,
    date::year year,
    std::optional<date::year> reportYear,
    std::string_view scenario,
    ScalingFactors sf,
    const fs::path& outputPath)
: _dataPath(dataPath)
, _outputPath(outputPath)
, _grid(grid)
, _runType(runType)
, _validation(validation)
, _year(year)
, _reportYear(reportYear)
, _scenario(scenario)
, _scalingFactors(std::move(sf))
{
}

fs::path RunConfiguration::emissions_dir_path() const
{
    return _dataPath / "emission data" / run_type_name(_runType) / std::to_string(static_cast<int>(_year));
}

fs::path RunConfiguration::point_source_emissions_path() const
{
    auto year = _reportYear.value_or(_year);
    return emissions_dir_path() / fmt::format("pointsource_emissions_{}.csv", static_cast<int>(_year));
}

fs::path RunConfiguration::total_emissions_path(EmissionSector::Type sectorType) const
{
    auto year = _reportYear.value_or(_year);
    return emissions_dir_path() / fmt::format("total_emissions_{}_{}.csv", sectorType, static_cast<int>(_year));
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

std::optional<date::year> RunConfiguration::reporting_year() const noexcept
{
    return _reportYear;
}

std::string_view RunConfiguration::scenario() const noexcept
{
    return _scenario;
}

const ScalingFactors& RunConfiguration::scaling_factors() const noexcept
{
    return _scalingFactors;
}

}
