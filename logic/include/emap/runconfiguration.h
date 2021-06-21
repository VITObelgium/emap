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
        std::optional<date::year> reportYear,
        std::string_view scenario,
        const fs::path& outputPath);

    fs::path point_source_emissions_path() const;
    fs::path total_emissions_path(EmissionSector::Type sectorType) const;

    fs::path diffuse_scalings_path() const;
    fs::path point_source_scalings_path() const;

    const fs::path& data_root() const noexcept;
    const fs::path& output_path() const noexcept;

    GridDefinition grid_definition() const noexcept;
    RunType run_type() const noexcept;
    ValidationType validation_type() const noexcept;

    date::year year() const noexcept;
    std::optional<date::year> reporting_year() const noexcept;

    std::string_view scenario() const noexcept;

private:
    fs::path emissions_dir_path() const;

    fs::path _dataPath;
    fs::path _outputPath;
    GridDefinition _grid;
    RunType _runType;
    ValidationType _validation;
    date::year _year;
    std::optional<date::year> _reportYear;
    std::string _scenario;
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
