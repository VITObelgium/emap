#include "emap/runconfiguration.h"

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
    const fs::path& spatialPatternsPath,
    GridDefinition grid,
    RunType runType,
    ValidationType validation,
    date::year year,
    std::optional<date::year> reportYear,
    std::string_view scenario,
    const fs::path& outputPath)
: _dataPath(dataPath)
, _spatialPatternsPath(spatialPatternsPath)
, _outputPath(outputPath)
, _grid(grid)
, _runType(runType)
, _validation(validation)
, _year(year)
, _reportYear(reportYear)
, _scenario(scenario)
{
}

fs::path RunConfiguration::emissions_dir_path() const
{
    return _dataPath / "emission data" / run_type_name(_runType) / std::to_string(static_cast<int>(_year));
}

fs::path RunConfiguration::point_source_emissions_path() const
{
    auto year = _reportYear.value_or(_year);
    return emissions_dir_path() / fmt::format("pointsource_emissions_{}.csv", static_cast<int>(year));
}

fs::path RunConfiguration::total_emissions_path(EmissionSector::Type sectorType) const
{
    auto year = _reportYear.value_or(_year);
    return emissions_dir_path() / fmt::format("total_emissions_{}_{}.csv", sectorType, static_cast<int>(year));
}

fs::path RunConfiguration::spatial_pattern_path(const EmissionIdentifier& emissionId) const
{
    return _spatialPatternsPath / fs::u8path(fmt::format("{}_{}_{}.tif", str::lowercase(emissionId.pollutant.code()), emissionId.sector.gnfr_name(), emissionId.country.code()));
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

void RunConfiguration::set_max_concurrency(int32_t concurrency) noexcept
{
    _concurrency = concurrency;
}

std::optional<int32_t> RunConfiguration::max_concurrency() const noexcept
{
    return _concurrency;
}

}
