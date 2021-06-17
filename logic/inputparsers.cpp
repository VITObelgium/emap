#include "emap/inputparsers.h"
#include "emap/emissions.h"
#include "emap/scalingfactors.h"
#include "infra/csvreader.h"
#include "infra/exception.h"
#include "infra/log.h"
#include "infra/string.h"

#include <cpl_port.h>
#include <exception>
#include <limits>
#include <string>
#include <utility>

namespace emap {

using namespace inf;

static int32_t required_csv_column(const inf::CsvReader& csv, const std::string& columnName)
{
    if (auto index = csv.column_index(columnName); index.has_value()) {
        return *index;
    }

    throw RuntimeError("Missing column '{}'", columnName);
}

static std::pair<int32_t, EmissionSector::Type> determine_sector_column(const inf::CsvReader& csv)
{
    if (auto index = csv.column_index("nfr_sector"); index.has_value()) {
        return std::make_pair(*index, EmissionSector::Type::Nfr);
    }

    if (auto index = csv.column_index("gnfr_sector"); index.has_value()) {
        return std::make_pair(*index, EmissionSector::Type::Gnfr);
    }

    throw RuntimeError("Missing nfr_sector or gnfr_sector column");
}

static EmissionType emission_type_from_string(std::string_view type)
{
    if (type == "historic") {
        return EmissionType::Historic;
    }

    if (type == "future") {
        return EmissionType::Future;
    }

    throw RuntimeError("Invalid emission type: {}", type);
}

static date::year to_year(std::string_view yearString)
{
    const auto year = str::to_int32(yearString);
    if (!year.has_value()) {
        throw RuntimeError("Invalid year value: {}", yearString);
    }

    date::year result(*year);
    if (!result.ok()) {
        throw RuntimeError("Invalid year value: {}", yearString);
    }

    return result;
}

static double to_double(std::string_view valueString)
{
    if (auto value = str::to_double(valueString); value.has_value()) {
        return *value;
    }

    if (valueString.empty()) {
        Log::warn("Empty emission value");
        return std::numeric_limits<double>::quiet_NaN();
    }

    throw RuntimeError("Invalid emission value: {}", valueString);
}

Emissions parse_emissions(const fs::path& emissionsCsv)
{
    // csv columns: type;scenario;year;reporting;country;nfr_sector|gnfr_sector;pollutant;emission;unit

    try {
        Emissions result;
        inf::CsvReader csv(emissionsCsv);

        auto colType                 = required_csv_column(csv, "type");
        auto colScenario             = required_csv_column(csv, "scenario");
        auto colYear                 = required_csv_column(csv, "year");
        auto colReporting            = required_csv_column(csv, "reporting");
        auto colCountry              = required_csv_column(csv, "country");
        auto colPollutant            = required_csv_column(csv, "pollutant");
        auto colEmission             = required_csv_column(csv, "emission");
        auto colUnit                 = required_csv_column(csv, "unit");
        auto [colSector, sectorType] = determine_sector_column(csv);

        for (auto& line : csv) {
            EmissionInfo info;
            info.type          = emission_type_from_string(line.get_string(colType));
            info.scenario      = line.get_string(colScenario);
            info.country       = line.get_string(colCountry);
            info.year          = to_year(line.get_string(colYear));
            info.reportingYear = to_year(line.get_string(colReporting));
            info.sector        = EmissionSector(sectorType, line.get_string(colSector));
            info.pollutant     = line.get_string(colPollutant);
            info.value         = EmissionValue(to_double(line.get_string(colEmission)), line.get_string(colUnit));
            result.add_emission(std::move(info));
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", emissionsCsv, e.what());
    }
}

ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv)
{
    // csv columns: country;nfr_sector;pollutant;factor

    try {
        ScalingFactors result;
        inf::CsvReader csv(scalingFactorsCsv);

        auto colCountry              = required_csv_column(csv, "country");
        auto colPollutant            = required_csv_column(csv, "pollutant");
        auto colFactor               = required_csv_column(csv, "factor");
        auto [colSector, sectorType] = determine_sector_column(csv);

        for (auto& line : csv) {
            ScalingFactor sf;
            sf.country   = line.get_string(colCountry);
            sf.pollutant = line.get_string(colPollutant);
            sf.sector    = EmissionSector(sectorType, line.get_string(colSector));
            sf.factor    = to_double(line.get_string(colFactor));
            result.add_scaling_factor(std::move(sf));
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", scalingFactorsCsv, e.what());
    }
}

}