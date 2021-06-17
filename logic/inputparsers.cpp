#include "emap/inputparsers.h"
#include "emap/emissions.h"
#include "infra/csvreader.h"
#include "infra/exception.h"
#include "infra/string.h"

#include <cpl_port.h>
#include <exception>
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
    auto year = str::to_int32(yearString);
    if (!year.has_value()) {
        throw RuntimeError("Invalid year value: {}", yearString);
    }

    date::year result(*year);
    if (!result.ok()) {
        throw RuntimeError("Invalid year value: {}", yearString);
    }

    return result;
}

Emissions parse_emissions(const fs::path& emissionsCsv)
{
    // csv columns: type;scenario;year;reporting;country;nfr_sector;pollutant;emission;unit

    try {
        Emissions result;
        inf::CsvReader csv(emissionsCsv);

        auto colType      = required_csv_column(csv, "type");
        auto colScenario  = required_csv_column(csv, "scenario");
        auto colYear      = required_csv_column(csv, "year");
        auto colReporting = required_csv_column(csv, "reporting");
        auto colCountry   = required_csv_column(csv, "country");
        auto colNfrSector = required_csv_column(csv, "nfr_sector");
        auto colPollutant = required_csv_column(csv, "pollutant");
        auto colEmission  = required_csv_column(csv, "emission");
        auto colUnit      = required_csv_column(csv, "unit");

        for (auto& line : csv) {
            EmissionInfo info;
            info.type          = emission_type_from_string(line.get_string(colType));
            info.scenario      = line.get_string(colScenario);
            info.country       = line.get_string(colCountry);
            info.year          = to_year(line.get_string(colYear));
            info.reportingYear = to_year(line.get_string(colReporting));
            info.nfrSector     = line.get_string(colNfrSector);
            info.pollutant     = line.get_string(colPollutant);
            info.value         = EmissionValue(line.get_double(colEmission).value(), line.get_string(colUnit));
            result.add_emission(std::move(info));
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", emissionsCsv, e.what());
    }
}

}