#pragma once

#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "infra/filesystem.h"

#include <date/date.h>
#include <fmt/core.h>
#include <optional>

namespace emap {

enum class RunType
{
    Emep,
    Gains,
};

enum class ValidationType
{
    NoValidation,
    SumValidation,
};

class RunConfiguration
{
public:
    RunConfiguration(
        const fs::path& dataPath,
        GridDefinition grid,
        RunType runType,
        ValidationType validation,
        date::year year,
        date::year reportYear,
        std::string_view scenario,
        SectorInventory sectors,
        PollutantInventory pollutants,
        CountryInventory countries,
        const fs::path& outputPath);

    fs::path point_source_emissions_path(const Country& country, const Pollutant& pol) const;
    fs::path total_emissions_path_nfr() const;
    fs::path total_emissions_path_gnfr() const;
    fs::path spatial_pattern_path() const;

    fs::path emission_output_raster_path(date::year year, const EmissionIdentifier& emissionId) const;
    fs::path emission_brn_output_path(date::year year, Pollutant pol, EmissionSector sector) const;

    fs::path diffuse_scalings_path() const;
    fs::path point_source_scalings_path() const;

    const fs::path& data_root() const noexcept;
    const fs::path& output_path() const noexcept;
    fs::path countries_vector_path() const noexcept;
    std::string country_field_id() const noexcept;
    fs::path run_summary_path() const;

    GridDefinition grid_definition() const noexcept;
    RunType run_type() const noexcept;
    ValidationType validation_type() const noexcept;

    date::year year() const noexcept;
    date::year reporting_year() const noexcept;

    std::string_view scenario() const noexcept;

    void set_max_concurrency(int32_t concurrency) noexcept;
    std::optional<int32_t> max_concurrency() const noexcept;

    const SectorInventory& sectors() const noexcept;
    const PollutantInventory& pollutants() const noexcept;
    const CountryInventory& countries() const noexcept;

private:
    fs::path emissions_dir_path() const;

    fs::path _dataPath;
    fs::path _outputPath;
    GridDefinition _grid;
    RunType _runType;
    ValidationType _validation;
    date::year _year;
    date::year _reportYear;
    std::string _scenario;
    SectorInventory _sectorInventory;
    PollutantInventory _pollutantInventory;
    CountryInventory _countryInventory;

    std::optional<int32_t> _concurrency;
};

std::string run_type_name(RunType type);

}

namespace fmt {
template <>
struct formatter<emap::RunType>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::RunType& value, FormatContext& ctx)
    {
        return format_to(ctx.out(), run_type_name(value));
    }
};
}
