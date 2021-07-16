#include "emap/inputparsers.h"
#include "emap/emissions.h"
#include "emap/scalingfactors.h"
#include "infra/csvreader.h"
#include "infra/exception.h"
#include "infra/gdal.h"
#include "infra/log.h"
#include "infra/string.h"
#include "unitconversion.h"

#include <cpl_port.h>
#include <exception>
#include <limits>
#include <string>
#include <unordered_set>
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
    return Pollutant::from_string(name);
}

static Country to_country(std::string_view name)
{
    return Country::from_string(name);
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

static double to_double(std::string_view valueString, size_t lineNr)
{
    if (auto value = str::to_double(valueString); value.has_value()) {
        return *value;
    }

    if (valueString.empty()) {
        Log::warn("Empty emission value on line {}", lineNr);
        return std::numeric_limits<double>::quiet_NaN();
    }

    throw RuntimeError("Invalid emission value: {}", valueString);
}

SingleEmissions parse_emissions(const fs::path& emissionsCsv)
{
    // csv columns: type;scenario;year;reporting;country;nfr_sector|gnfr_sector;pollutant;emission;unit
    // pointsource csv columns: type;scenario;year;reporting;country;nfr-sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;Debiet_Nm3/u;Type emissie omschrijving;EIL-nummer;Exploitatie naam;NACE-code;EIL Emissiepunt Jaar Naam;Activiteit type

    try {
        Log::debug("Parse emissions: {}", emissionsCsv);

        SingleEmissions result;
        inf::CsvReader csv(emissionsCsv);

        auto colCountry              = required_csv_column(csv, "country");
        auto colPollutant            = required_csv_column(csv, "pollutant");
        auto colEmission             = required_csv_column(csv, "emission");
        auto colUnit                 = required_csv_column(csv, "unit");
        auto [colSector, sectorType] = determine_sector_column(csv);
        auto colX                    = csv.column_index("x");
        auto colY                    = csv.column_index("y");

        size_t lineNr = 2;
        for (auto& line : csv) {
            double emissionValue = to_giga_gram(to_double(line.get_string(colEmission), lineNr), line.get_string(colUnit));

            EmissionEntry info(
                EmissionIdentifier(to_country(line.get_string(colCountry)), to_sector(sectorType, line.get_string(colSector)), to_pollutant(line.get_string(colPollutant))),
                EmissionValue(emissionValue));

            if (colX.has_value() && colY.has_value()) {
                auto x = line.get_int32(*colX);
                auto y = line.get_int32(*colY);
                if (x.has_value() && y.has_value()) {
                    info.set_coordinate(Coordinate(*x, *y));
                } else {
                    throw RuntimeError("Invalid coordinate in point sources: {}", line.get_string(*colX), line.get_string(*colY));
                }
            }

            result.add_emission(std::move(info));
            ++lineNr;
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
        Log::debug("Parse scaling factors: {}", scalingFactorsCsv);

        ScalingFactors result;
        inf::CsvReader csv(scalingFactorsCsv);

        auto colCountry              = required_csv_column(csv, "country");
        auto colPollutant            = required_csv_column(csv, "pollutant");
        auto colFactor               = required_csv_column(csv, "factor");
        auto [colSector, sectorType] = determine_sector_column(csv);

        size_t lineNr = 2;
        for (auto& line : csv) {
            const auto country   = to_country(line.get_string(colCountry));
            const auto sector    = to_sector(sectorType, line.get_string(colSector));
            const auto pollutant = to_pollutant(line.get_string(colPollutant));
            const auto factor    = to_double(line.get_string(colFactor), lineNr);

            if (factor != 1.0) {
                result.add_scaling_factor(ScalingFactor(EmissionIdentifier(country, sector, pollutant), factor));
            }
            ++lineNr;
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", scalingFactorsCsv, e.what());
    }
}

SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData)
{
    SingleEmissions result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(emissionsData);
    auto layer = ds.layer(0);

    auto colX         = layer.layer_definition().required_field_index("Emissiepunt X Coördinaat (Lambert)");
    auto colY         = layer.layer_definition().required_field_index("Emissiepunt Y Coördinaat (Lambert)");
    auto colEmission  = layer.layer_definition().required_field_index("Emissie");
    auto colUnit      = layer.layer_definition().required_field_index("Eenheid emissie");
    auto colSector    = layer.layer_definition().required_field_index("IPCC NFR Code");
    auto colPollutant = layer.layer_definition().required_field_index("Verontreinigende stof");

    for (auto& feature : layer) {
        if (feature.field_as<std::string_view>(colPollutant).empty()) {
            continue; // skip empty rows
        }

        EmissionEntry info(
            EmissionIdentifier(Country::Id::BEF,
                               to_sector(EmissionSector::Type::Nfr, feature.field_as<std::string_view>(colSector)),
                               to_pollutant(feature.field_as<std::string_view>(colPollutant))),
            EmissionValue(to_giga_gram(feature.field_as<double>(colEmission), feature.field_as<std::string_view>(colUnit))));

        info.set_coordinate(Coordinate(feature.field_as<int32_t>(colX), feature.field_as<int32_t>(colY)));
        result.add_emission(std::move(info));
    }

    return result;
}

SingleEmissions parse_emissions_flanders(const fs::path& emissionsData)
{
    SingleEmissions result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(emissionsData);
    auto layer = ds.layer(0);

    auto colX         = layer.layer_definition().required_field_index("KM2 Grid Lambert X Coördinaat");
    auto colY         = layer.layer_definition().required_field_index("KM2 Grid Lambert Y Coördinaat");
    auto colEmission  = layer.layer_definition().required_field_index("Emissie");
    auto colUnit      = layer.layer_definition().required_field_index("Eenheid Symbool");
    auto colSector    = layer.layer_definition().required_field_index("IPCC NFR Code");
    auto colPollutant = layer.layer_definition().required_field_index("Parameter Symbool");

    static const std::unordered_set<std::string_view> memoSectors{{
        "1A3ai(ii)",
        "1A3aii(ii)",
        "1A3di(i)",
        "1A5c"
        "6B",
        "11A",
        "11B",
        "11C",
    }};

    for (auto& feature : layer) {
        auto sectorName = feature.field_as<std::string_view>(colSector);
        if (sectorName.empty() || memoSectors.count(sectorName) > 0) {
            continue;
        }

        EmissionEntry info(
            EmissionIdentifier(Country::Id::BEF,
                               to_sector(EmissionSector::Type::Nfr, sectorName),
                               to_pollutant(feature.field_as<std::string_view>(colPollutant))),
            EmissionValue(to_giga_gram(feature.field_as<double>(colEmission), feature.field_as<std::string_view>(colUnit))));

        info.set_coordinate(Coordinate(feature.field_as<int32_t>(colX), feature.field_as<int32_t>(colY)));
        result.add_emission(std::move(info));
    }

    return result;
}

}
