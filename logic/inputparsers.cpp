#include "emap/inputparsers.h"
#include "emap/constants.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/inputconversion.h"
#include "emap/runconfiguration.h"
#include "emap/scalingfactors.h"
#include "infra/csvreader.h"
#include "infra/enumutils.h"
#include "infra/exception.h"
#include "infra/gdal.h"
#include "infra/log.h"
#include "infra/string.h"
#include "unitconversion.h"

#include <cassert>
#include <cpl_port.h>
#include <exception>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4267 4244)
#endif
#include <csv.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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

SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg)
{
    // csv columns: type;scenario;year;reporting_country;nfr_sector|gnfr_sector;pollutant;emission;unit
    // pointsource csv columns: type;scenario;year;reporting_country;nfr-sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;Debiet_Nm3/u;Type emissie omschrijving;EIL-nummer;Exploitatie naam;NACE-code;EIL Emissiepunt Jaar Naam;Activiteit type

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    try {
        Log::debug("Parse emissions: {}", emissionsCsv);

        SingleEmissions result;
        inf::CsvReader csv(emissionsCsv);

        auto colCountry              = required_csv_column(csv, "reporting_country");
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
                EmissionIdentifier(countryInv.country_from_string(line.get_string(colCountry)), sectorInv.sector_from_string(sectorType, line.get_string(colSector)), pollutantInv.pollutant_from_string(line.get_string(colPollutant))),
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

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, const RunConfiguration& cfg)
{
    // First lines are comments
    // Format: ISO2;YEAR;SECTOR;POLLUTANT;UNIT;NUMBER/FLAG

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    try {
        SingleEmissions result;
        Log::debug("Parse emissions: {}", emissionsCsv);

        using namespace io;
        CSVReader<6, trim_chars<' ', '\t'>, no_quote_escape<';'>, throw_on_overflow, single_line_comment<'#'>> in(emissionsCsv.u8string());

        int year;
        char *countryStr, *sector, *pollutant, *unit, *value;
        while (in.read_row(countryStr, year, sector, pollutant, unit, value)) {
            auto emissionValue = str::to_double(value);
            if (emissionValue.has_value()) {
                *emissionValue = to_giga_gram(*emissionValue, unit);
            }

            const auto country = countryInv.try_country_from_string(countryStr);
            if (!country.has_value()) {
                // not interested in this country, no need to report this
                continue;
            }

            try {
                if (!sectorInv.is_ignored_sector(sector)) {
                    EmissionEntry info(
                        EmissionIdentifier(*country, sectorInv.sector_from_string(sectorType, sector), pollutantInv.pollutant_from_string(pollutant)), EmissionValue(emissionValue));

                    result.add_emission(std::move(info));
                }
            } catch (const std::exception& e) {
                Log::debug("Ignoring line {} in {} ({})", in.get_file_line(), emissionsCsv, e.what());
            }
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", emissionsCsv, e.what());
    }
}

ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv, const RunConfiguration& cfg)
{
    // csv columns: country;nfr_sector;pollutant;factor

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

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
            const auto country   = countryInv.country_from_string(line.get_string(colCountry));
            const auto sector    = sectorInv.sector_from_string(sectorType, line.get_string(colSector));
            const auto pollutant = pollutantInv.pollutant_from_string(line.get_string(colPollutant));
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

static Country detect_belgian_region_from_filename(const fs::path& path)
{
    auto filename = path.stem().u8string();
    if (str::starts_with(filename, "BEB")) {
        return country::BEB; // Brussels
    } else if (str::starts_with(filename, "BEF")) {
        return country::BEF; // Flanders
    } else if (str::starts_with(filename, "BEW")) {
        return country::BEW; // Wallonia
    }

    throw RuntimeError("Could not detect region from filename: {}", filename);
}

static std::optional<Pollutant> detect_pollutant_name_from_header(std::string_view hdr, const PollutantInventory& pollutantInv)
{
    std::optional<Pollutant> pol;

    try {
        pol = pollutantInv.pollutant_from_string(hdr);
    } catch (const std::exception&) {
    }

    return pol;
}

static std::string_view strip_newline(std::string_view str)
{
    auto iter = str.find_first_of('\n');
    if (iter != std::string::npos) {
        return str::trimmed_view(str.substr(0, iter));
    }

    return str;
}

SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const RunConfiguration& cfg)
{
    SingleEmissions result;

    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    const auto country = detect_belgian_region_from_filename(emissionsData);

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "DISABLE");
    auto ds    = gdal::VectorDataSet::open(emissionsData);
    auto layer = ds.layer(std::to_string(static_cast<int>(year)));

    constexpr const int pollutantLineNr = 12;
    constexpr const int unitLineNr      = pollutantLineNr + 1;

    struct PollutantData
    {
        PollutantData(Pollutant pol) noexcept
        : pollutant(pol)
        {
        }

        Pollutant pollutant;
        double unitConversion = 1.0;
    };

    std::unordered_map<int, PollutantData> pollutantColumns;
    std::unordered_map<NfrId, int32_t> usedSectorPriories;

    int lineNr = 0;
    for (const auto& feature : layer) {
        ++lineNr;

        if (lineNr == pollutantLineNr) {
            for (int i = 0; i < feature.field_count(); ++i) {
                if (auto pol = detect_pollutant_name_from_header(strip_newline(feature.field_as<std::string_view>(i)), pollutantInv); pol.has_value()) {
                    pollutantColumns.emplace(i, *pol);
                }
            }
        } else if (lineNr == unitLineNr) {
            for (auto& [index, pol] : pollutantColumns) {
                pol.unitConversion = to_giga_gram_factor(feature.field_as<std::string_view>(index)).value_or(1.0);
            }
        }

        if (auto nfrSectorName = feature.field_as<std::string_view>(1); !nfrSectorName.empty()) {
            EmissionSector nfrSector;
            bool sectorOverride = false;

            try {
                auto [sector, priority] = sectorInv.nfr_sector_with_priority_from_string(nfrSectorName);
                nfrSector               = EmissionSector(sector);

                if (auto iter = usedSectorPriories.find(sector.id()); iter != usedSectorPriories.end()) {
                    // Sector was already processed, check if the current priority is higher
                    if (priority > iter->second) {
                        // the current entry has a higher priority, update the map
                        iter->second   = priority;
                        sectorOverride = true;
                    } else {
                        // the current entry has a lower priority priority, skip it
                        continue;
                    }
                } else {
                    // first time we encounter this sector, add the current priority
                    usedSectorPriories.emplace(sector.id(), priority);
                }
            } catch (const std::exception&) {
                // not an nfr value line, skipping
                continue;
            }

            if (pollutantColumns.empty()) {
                throw RuntimeError("Invalid format: Sector appears before the Pollutant header");
            }

            std::optional<double> pm10, pm2_5;

            for (const auto& [index, polData] : pollutantColumns) {
                int i            = index;
                const auto field = feature.field(index);
                std::optional<double> emissionValue;
                if (const auto* emission = std::get_if<double>(&field)) {
                    emissionValue = (*emission) * polData.unitConversion;
                } else if (const auto* emission = std::get_if<std::string_view>(&field)) {
                    emissionValue = str::to_double(*emission);
                    if (emissionValue.has_value()) {
                        emissionValue = (*emissionValue) * polData.unitConversion;
                    }
                }

                if (emissionValue.has_value()) {
                    if (polData.pollutant.code() == "PM10") {
                        pm10 = emissionValue;
                    } else if (polData.pollutant.code() == "PM2.5") {
                        pm2_5 = emissionValue;
                    }

                    if (sectorOverride) {
                        // update the existing emission with the higher priority version
                        result.update_or_add_emission(EmissionEntry(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue)));
                    } else {
                        result.add_emission(EmissionEntry(
                            EmissionIdentifier(country, nfrSector, polData.pollutant),
                            EmissionValue(*emissionValue)));
                    }
                } else {
                    const auto value = feature.field_as<std::string>(index);
                    if (!value.empty() && value != "NO" && value != "IE" && value != "NA" && value != "NE" && value != "NR" && value != "C") {
                        Log::error("Failed to obtain emission value from {}", value);
                    }
                }
            }

            if (const auto pmCoarse = pollutantInv.try_pollutant_from_string(constants::pollutant::PMCoarse); pmCoarse.has_value()) {
                // This config has a PMCoarse pollutant, add it as difference of PM10 and PM2.5

                if (pm2_5.has_value() && pm10.has_value()) {
                    if (pm10 >= pm2_5) {
                        auto pmcVal = EmissionValue(*pm10 - *pm2_5);
                        if (sectorOverride) {
                            // update the existing emission with the higher priority version
                            result.update_or_add_emission(EmissionEntry(EmissionIdentifier(country, nfrSector, *pmCoarse), pmcVal));
                        } else {
                            result.add_emission(EmissionEntry(
                                EmissionIdentifier(country, nfrSector, *pmCoarse),
                                pmcVal));
                        }
                    } else {
                        throw RuntimeError("Invalid PM data for sector {} (PM10: {}, PM2.5 {})", nfrSector, *pm10, *pm2_5);
                    }
                }
            }
        }
    }

    return result;
}

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const RunConfiguration& cfg)
{
    std::vector<SpatialPatternData> result;

    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(spatialPatternPath);
    auto layer = ds.layer(0);
    std::optional<date::year> year;

    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    const auto gridData  = grid_data(GridDefinition::Flanders1km);

    EmissionIdentifier id;
    id.country = country::BEF;

    auto colYear      = layer.layer_definition().required_field_index("year");
    auto colSector    = layer.layer_definition().required_field_index("nfr_sector");
    auto colPollutant = layer.layer_definition().required_field_index("pollutant");
    auto colX         = layer.layer_definition().required_field_index("x_lambert");
    auto colY         = layer.layer_definition().required_field_index("y_lambert");
    auto colEmission  = layer.layer_definition().required_field_index("emission");
    //auto colUnit      = layer.layer_definition().required_field_index("unit");

    std::optional<EmissionSector> currentSector;
    gdx::DenseRaster<double> currentRaster(gridData.meta, gridData.meta.nodata.value());
    std::unordered_set<std::string> invalidSectors;
    for (const auto& feature : layer) {
        year         = date::year(feature.field_as<int>(colYear));
        id.pollutant = pollutantInv.pollutant_from_string(feature.field_as<std::string_view>(colPollutant));

        try {
            id.sector = EmissionSector(sectorInv.nfr_sector_from_string(feature.field_as<std::string_view>(colSector)));
        } catch (const std::exception& e) {
            std::string sector(feature.field_as<std::string_view>(colSector));
            if (invalidSectors.count(sector) == 0) {
                invalidSectors.emplace(feature.field_as<std::string_view>(colSector));
                Log::warn(e.what());
            }
            continue;
        }

        if (currentSector != id.sector) {
            if (currentSector.has_value()) {
                // Store the current raster and start a new one

                SpatialPatternData spData;
                spData.id     = EmissionIdentifier(id.country, *currentSector, id.pollutant);
                spData.year   = *year;
                spData.raster = std::move(currentRaster);
                result.push_back(std::move(spData));

                // reset the raster
                currentRaster = gdx::DenseRaster<double>(gridData.meta, gridData.meta.nodata.value());
            }

            currentSector = id.sector;
        }

        const Point<double> point(feature.field_as<double>(colX), feature.field_as<double>(colY));
        const Cell cell = gridData.meta.convert_point_to_cell(point);
        if (gridData.meta.is_on_map(cell)) {
            currentRaster[cell] = feature.field_as<double>(colEmission);
        } else {
            Log::warn("Point outside of flanders extent: {}", point);
        }
    }

    if (year.has_value()) {
        SpatialPatternData spData;
        spData.id     = id;
        spData.year   = *year;
        spData.raster = std::move(currentRaster);
        result.push_back(std::move(spData));
    }

    return result;
}

}
