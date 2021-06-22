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

static Pollutant to_pollutant(std::string_view name)
{
    return pollutant_from_string(name);
}

static Country to_country(std::string_view name)
{
    return country_from_string(name);
}

static EmissionSector to_sector(EmissionSector::Type type, std::string_view name)
{
    switch (type) {
    case EmissionSector::Type::Nfr:
        return EmissionSector(nfr_sector_from_string(name));
    case EmissionSector::Type::Gnfr:
        return EmissionSector(gnfr_sector_from_string(name));
    default:
        throw RuntimeError("Invalid sector type");
    }
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
    // pointsource csv columns: type;scenario;year;reporting;country;nfr-sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;Debiet_Nm3/u;Type emissie omschrijving;EIL-nummer;Exploitatie naam;NACE-code;EIL Emissiepunt Jaar Naam;Activiteit type

    try {
        Emissions result;
        inf::CsvReader csv(emissionsCsv);

        auto colCountry              = required_csv_column(csv, "country");
        auto colPollutant            = required_csv_column(csv, "pollutant");
        auto colEmission             = required_csv_column(csv, "emission");
        auto colUnit                 = required_csv_column(csv, "unit");
        auto [colSector, sectorType] = determine_sector_column(csv);
        auto colX                    = csv.column_index("x");
        auto colY                    = csv.column_index("y");

        for (auto& line : csv) {
            EmissionInfo info;
            info.country   = line.get_string(colCountry);
            info.sector    = to_sector(sectorType, line.get_string(colSector));
            info.pollutant = to_pollutant(line.get_string(colPollutant));
            info.value     = EmissionValue(to_double(line.get_string(colEmission)), line.get_string(colUnit));

            if (colX.has_value() && colY.has_value()) {
                auto x = line.get_int32(*colX);
                auto y = line.get_int32(*colY);
                if (x.has_value() && y.has_value()) {
                    info.coordinate = Coordinate(*x, *y);
                } else {
                    throw RuntimeError("Invalid coordinate in point sources: {}", line.get_string(*colX), line.get_string(*colY));
                }
            }

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
            sf.country   = to_country(line.get_string(colCountry));
            sf.pollutant = to_pollutant(line.get_string(colPollutant));
            sf.sector    = to_sector(sectorType, line.get_string(colSector));
            sf.factor    = to_double(line.get_string(colFactor));
            result.add_scaling_factor(std::move(sf));
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", scalingFactorsCsv, e.what());
    }
}

}
