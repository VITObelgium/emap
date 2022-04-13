#include "emap/inputparsers.h"
#include "emap/constants.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/inputconversion.h"
#include "emap/runconfiguration.h"
#include "emap/scalingfactors.h"
#include "emap/sector.h"
#include "infra/csvreader.h"
#include "infra/enumutils.h"
#include "infra/exception.h"
#include "infra/gdal.h"
#include "infra/hash.h"
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

static std::optional<double> parse_emission_value(std::string_view emission)
{
    if (emission == "NO" || emission == "IE" || emission == "NA" || emission == "NE" || emission == "NR" || emission == "C") {
        return {};
    }

    return str::to_double(emission);
}

static void update_entry(std::vector<EmissionEntry>& entries, const EmissionEntry& newEntry)
{
    auto entryIter = std::find_if(entries.begin(), entries.end(), [&](const EmissionEntry& entry) {
        return entry.id() == newEntry.id();
    });

    assert(entryIter != entries.end());
    *entryIter = newEntry;
}

SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg)
{
    // pointsource csv columns: type;scenario;year;reporting_country;nfr-sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;Debiet_Nm3/u;Type emissie omschrijving;EIL-nummer;Exploitatie naam;NACE-code;EIL Emissiepunt Jaar Naam;Activiteit type;subtype

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    size_t lineNr = 2;

    try {
        Log::debug("Parse emissions: {}", emissionsCsv);

        /*
        using namespace io;
        CSVReader<21, trim_chars<' ', '\t'>, no_quote_escape<';'>, throw_on_overflow> in(str::from_u8(emissionsCsv.u8string()));


        int32_t year;
        double value, height, diameter, temp, warmthContents, x, y, flowRate;
        char *type, *scenario, *countryStr, *sectorName, *pollutantName, *unit, *excType, *eilNr, *explName, *naceCode, *eilYearName, *activityType, *subtype;
        while (in.read_row(type, scenario, year, countryStr, sectorName, pollutant, value, x, y, unit, height, diameter, temp, warmthContents, flowRate, excType, eilNr, explName, naceCode, eilYearName, activityType, subtype)) {
            if (sectorInv.is_ignored_sector(sectorType, sectorName) ||
                pollutantInv.is_ignored_pollutant(pollutantName)) {
                continue;
            }

            double emissionValue = to_giga_gram(to_double(line.get_string(colEmission), lineNr), line.get_string(colUnit));

            auto sector    = sectorInv.try_sector_from_string(sectorType, sectorName);
            auto country   = countryInv.try_country_from_string(line.get_string(colCountry));
            auto pollutant = pollutantInv.try_pollutant_from_string(pollutantName);
            if (sector.has_value() && country.has_value() && pollutant.has_value()) {
                EmissionEntry info(
                    EmissionIdentifier(*country, *sector, *pollutant),
                    EmissionValue(emissionValue));

                info.set_height(line.get_double(colHeight).value_or(0.0));
                info.set_diameter(line.get_double(colDiameter).value_or(0.0));
                info.set_temperature(line.get_double(colTemperature).value_or(-9999.0));
                info.set_warmth_contents(line.get_double(colWarmthContents).value_or(0.0));
                info.set_flow_rate(line.get_double(colFlowRate).value_or(0.0));

                std::string subType = "none";
                if (colSubType.has_value()) {
                    subType = line.get_string(*colSubType);
                }

                info.set_source_id(fmt::format("{}_{}_{}_{}_{}_{}_{}_{}", info.height(), info.diameter(), info.temperature(), info.warmth_contents(), info.flow_rate(), line.get_string(colEilPoint), line.get_string(colEil), subType));

                if (colX.has_value() && colY.has_value()) {
                    auto x = line.get_double(*colX);
                    auto y = line.get_double(*colY);
                    if (x.has_value() && y.has_value()) {
                        info.set_coordinate(Coordinate(*x, *y));
                    } else {
                        throw RuntimeError("Invalid coordinate in point sources: {}", line.get_string(*colX), line.get_string(*colY));
                    }
                }

                result.add_emission(std::move(info));
            }
            ++lineNr;
        }*/

        SingleEmissions result(cfg.year());
        inf::CsvReader csv(emissionsCsv);

        auto colCountry   = required_csv_column(csv, "reporting_country");
        auto colPollutant = required_csv_column(csv, "pollutant");
        auto colEmission  = required_csv_column(csv, "emission");
        auto colUnit      = required_csv_column(csv, "unit");

        auto colHeight         = required_csv_column(csv, "hoogte_m");
        auto colDiameter       = required_csv_column(csv, "diameter_m");
        auto colTemperature    = required_csv_column(csv, "temperatuur_C");
        auto colWarmthContents = required_csv_column(csv, "warmteinhoud_MW");
        auto colFlowRate       = required_csv_column(csv, "debiet_Nm3/u");
        auto colEil            = required_csv_column(csv, "EIL_nummer");
        auto colEilPoint       = required_csv_column(csv, "EIL_Emissiepunt_Jaar_Naam");

        auto colSubType = csv.column_index("subtype");

        auto [colSector, sectorType] = determine_sector_column(csv);
        auto colX                    = csv.column_index("x");
        auto colY                    = csv.column_index("y");
        auto colDv                   = csv.column_index("dv");

        for (auto& line : csv) {
            const auto sectorName    = line.get_string(colSector);
            const auto pollutantName = line.get_string(colPollutant);
            if (sectorName.empty() || sectorInv.is_ignored_sector(sectorType, sectorName) ||
                pollutantInv.is_ignored_pollutant(pollutantName)) {
                continue;
            }

            double emissionValue = to_giga_gram(to_double(line.get_string(colEmission), lineNr), line.get_string(colUnit));

            auto sector    = sectorInv.try_sector_from_string(sectorType, sectorName);
            auto country   = countryInv.try_country_from_string(line.get_string(colCountry));
            auto pollutant = pollutantInv.try_pollutant_from_string(pollutantName);
            if (sector.has_value() && country.has_value() && pollutant.has_value()) {
                EmissionEntry info(
                    EmissionIdentifier(*country, *sector, *pollutant),
                    EmissionValue(emissionValue));

                info.set_height(line.get_double(colHeight).value_or(0.0));
                info.set_diameter(line.get_double(colDiameter).value_or(0.0));
                info.set_temperature(line.get_double(colTemperature).value_or(-9999.0));
                info.set_warmth_contents(line.get_double(colWarmthContents).value_or(0.0));
                info.set_flow_rate(line.get_double(colFlowRate).value_or(0.0));

                std::string subType = "none";
                if (colSubType.has_value()) {
                    subType = line.get_string(*colSubType);
                }

                info.set_source_id(fmt::format("{}_{}_{}_{}_{}_{}_{}_{}", info.height(), info.diameter(), info.temperature(), info.warmth_contents(), info.flow_rate(), line.get_string(colEilPoint), line.get_string(colEil), subType));

                if (colX.has_value() && colY.has_value()) {
                    auto x = line.get_double(*colX);
                    auto y = line.get_double(*colY);
                    if (x.has_value() && y.has_value()) {
                        info.set_coordinate(Coordinate(*x, *y));
                    } else {
                        throw RuntimeError("Invalid coordinate in point sources: {}", line.get_string(*colX), line.get_string(*colY));
                    }
                }

                if (colDv.has_value()) {
                    info.set_dv(line.get_int32(*colDv));
                }

                result.add_emission(std::move(info));
            }
            ++lineNr;
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} line {} ({})", emissionsCsv, lineNr, e.what());
    }
}

static SingleEmissions calculate_pmcoarse_emissions(const PollutantInventory& inv, const SingleEmissions& emissions)
{
    std::vector<EmissionEntry> entries;

    try {
        auto pm10     = inv.pollutant_from_string(constants::pollutant::PM10);
        auto pm2_5    = inv.pollutant_from_string(constants::pollutant::PM2_5);
        auto pmCoarse = inv.pollutant_from_string(constants::pollutant::PMCoarse);

        for (auto& em : emissions) {
            if (em.pollutant() == pm10 && em.value().amount().has_value()) {
                auto pm25Id      = em.id();
                pm25Id.pollutant = pm2_5;

                double pm25Value = 0.0;
                if (auto pm2_5Emission = emissions.try_emission_with_id(em.id().with_pollutant(pm2_5)); pm2_5Emission.has_value()) {
                    pm25Value = pm2_5Emission->value().amount().value_or(0.0);
                }

                entries.emplace_back(em.id().with_pollutant(pmCoarse), EmissionValue((*em.value().amount()) - pm25Value));
            }
        }
    } catch (const std::exception&) {
        // Not all required pollutants are part of this configuration
    }

    return SingleEmissions(emissions.year(), std::move(entries));
}

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, date::year requestYear, const RunConfiguration& cfg)
{
    // First lines are comments
    // Format: ISO2;YEAR;SECTOR;POLLUTANT;UNIT;NUMBER/FLAG

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();

    try {
        Log::debug("Parse emissions: {}", emissionsCsv);
        std::vector<EmissionEntry> entries;
        std::unordered_map<EmissionIdentifier, int32_t> usedSectorPriories;

        using namespace io;
        CSVReader<6, trim_chars<' ', '\t'>, no_quote_escape<';'>, throw_on_overflow, single_line_comment<'#'>> in(str::from_u8(emissionsCsv.u8string()));

        int32_t year;
        char *countryStr, *sectorName, *pollutant, *unit, *value;
        while (in.read_row(countryStr, year, sectorName, pollutant, unit, value)) {
            if (year != static_cast<int32_t>(requestYear)) {
                // not the year we want
                continue;
            }

            auto emissionValue = parse_emission_value(value).value_or(0.0);
            emissionValue      = to_giga_gram(emissionValue, unit);

            const auto country = countryInv.try_country_from_string(countryStr);
            if (!country.has_value()) {
                // not interested in this country, no need to report this
                continue;
            }

            try {
                if (!sectorInv.is_ignored_sector(sectorType, sectorName) && !pollutantInv.is_ignored_pollutant(pollutant)) {
                    auto [sector, priority] = sectorInv.sector_with_priority_from_string(sectorType, sectorName);
                    EmissionIdentifier id(*country, sectorInv.sector_from_string(sectorType, sectorName), pollutantInv.pollutant_from_string(pollutant));

                    EmissionEntry info(id, EmissionValue(emissionValue));

                    if (auto iter = usedSectorPriories.find(id); iter != usedSectorPriories.end()) {
                        // Sector was already processed, check if the current priority is higher
                        if (priority > iter->second && emissionValue > 0.0) {
                            // the current entry has a higher priority, update the map
                            update_entry(entries, info);
                        } else {
                            // the current entry has a lower priority priority, skip it
                            continue;
                        }
                    } else {
                        // first time we encounter this sector, add the current priority
                        usedSectorPriories.emplace(id, priority);
                        entries.push_back(info);
                    }
                }
            } catch (const std::exception& e) {
                Log::debug("Ignoring line {} in {} ({})", in.get_file_line(), emissionsCsv, e.what());
            }
        }

        SingleEmissions emissions(requestYear, std::move(entries));
        merge_unique_emissions(emissions, calculate_pmcoarse_emissions(pollutantInv, emissions));
        return emissions;
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
    auto filename = path.stem().string();
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
        if (!pollutantInv.is_ignored_pollutant(hdr)) {
            pol = pollutantInv.pollutant_from_string(hdr);
        }
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

    std::vector<EmissionEntry> entries;

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
            if (sectorInv.is_ignored_nfr_sector(nfrSectorName)) {
                continue;
            }

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
                const auto field = feature.field(index);
                std::optional<double> emissionValue;
                if (const auto* emission = std::get_if<double>(&field)) {
                    emissionValue = (*emission) * polData.unitConversion;
                } else if (const auto* emission = std::get_if<std::string_view>(&field)) {
                    emissionValue = parse_emission_value(*emission);
                    if (!emissionValue.has_value()) {
                        emissionValue = 0.0;
                    } else {
                        emissionValue = (*emissionValue) * polData.unitConversion;
                    }
                }

                if (emissionValue.has_value()) {
                    if (polData.pollutant.code() == constants::pollutant::PM10) {
                        pm10 = emissionValue;
                    } else if (polData.pollutant.code() == constants::pollutant::PM2_5) {
                        pm2_5 = emissionValue;
                    }

                    if (sectorOverride) {
                        // update the existing emission with the higher priority version
                        update_entry(entries, EmissionEntry(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue)));
                    } else {
                        entries.emplace_back(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue));
                    }
                } else {
                    const auto value = feature.field_as<std::string>(index);
                    if (!value.empty()) {
                        Log::error("Failed to obtain emission value from {}", value);
                    }
                }
            }

            if (const auto pmCoarse = pollutantInv.try_pollutant_from_string(constants::pollutant::PMCoarse); pmCoarse.has_value()) {
                // This config has a PMCoarse pollutant, add it as difference of PM10 and PM2.5

                if (pm10.has_value()) {
                    if (*pm10 >= pm2_5.value_or(0.0)) {
                        auto pmcVal = EmissionValue(*pm10 - pm2_5.value_or(0.0));
                        if (sectorOverride) {
                            // update the existing emission with the higher priority version
                            update_entry(entries, EmissionEntry(EmissionIdentifier(country, nfrSector, *pmCoarse), pmcVal));
                        } else {
                            entries.emplace_back(EmissionIdentifier(country, nfrSector, *pmCoarse), pmcVal);
                        }
                    } else {
                        throw RuntimeError("Invalid PM data for sector {} (PM10: {}, PM2.5 {})", nfrSector, *pm10, pm2_5.value_or(0.0));
                    }
                }
            }
        }
    }

    return SingleEmissions(year, entries);
}

static std::optional<EmissionSector> emission_sector_from_feature(const gdal::Feature& feature, int colNfr, int colGnfr, const SectorInventory& sectorInv)
{
    try {
        std::string nfrSectorName(str::trimmed_view(feature.field_as<std::string_view>(colNfr)));
        if (!nfrSectorName.empty()) {
            // Nfr sector
            if (sectorInv.is_ignored_nfr_sector(nfrSectorName)) {
                return {};
            }

            return EmissionSector(sectorInv.nfr_sector_from_string(nfrSectorName));
        } else if (colGnfr >= 0) {
            // Gnfr sector
            std::string gnfrSectorName(str::trimmed_view(feature.field_as<std::string_view>(colGnfr)));
            if (sectorInv.is_ignored_gnfr_sector(gnfrSectorName)) {
                return {};
            }

            return EmissionSector(sectorInv.gnfr_sector_from_string(gnfrSectorName));
        }
    } catch (const std::exception& e) {
        Log::warn(e.what());
    }

    return {};
}

static Cell cell_for_emission_feature(const gdal::Feature& feature, int colX, int colY, const GeoMetadata& meta)
{
    const double centerOffsetX = meta.cell_size_x() / 2.0;
    const double centerOffsetY = (-meta.cell_size_y()) / 2.0;

    // Coordinates are lower left cell corners: put the point in the cell center for determining the cell
    const Point<double> point(feature.field_as<double>(colX) + centerOffsetX, feature.field_as<double>(colY) + centerOffsetY);
    return meta.convert_point_to_cell(point);
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

    const auto gridData = grid_data(GridDefinition::Flanders1km);

    EmissionIdentifier id;
    id.country = country::BEF;

    auto colYear       = layer.layer_definition().required_field_index("year");
    auto colNfrSector  = layer.layer_definition().required_field_index("nfr_sector");
    auto colGnfrSector = layer.layer_definition().field_index("gnfr_sector");
    auto colPollutant  = layer.layer_definition().required_field_index("pollutant");
    auto colX          = layer.layer_definition().required_field_index("x_lambert");
    auto colY          = layer.layer_definition().required_field_index("y_lambert");
    auto colEmission   = layer.layer_definition().required_field_index("emission");

    std::optional<EmissionSector> currentSector;
    gdx::DenseRaster<double> currentRaster(gridData.meta, gridData.meta.nodata.value());
    std::unordered_set<std::string> invalidSectors;
    for (const auto& feature : layer) {
        if (!feature.field_is_valid(colYear)) {
            continue; // skip empy lines
        }

        if (auto sector = emission_sector_from_feature(feature, colNfrSector, colGnfrSector, sectorInv); sector.has_value()) {
            id.sector    = *sector;
            id.pollutant = pollutantInv.pollutant_from_string(feature.field_as<std::string_view>(colPollutant));
            year         = date::year(feature.field_as<int>(colYear));

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

            const auto cell = cell_for_emission_feature(feature, colX, colY, gridData.meta);
            if (gridData.meta.is_on_map(cell)) {
                currentRaster[cell] = feature.field_as<double>(colEmission);
            } else {
                Log::warn("Point outside of flanders extent: {}", Point(feature.field_as<double>(colX), feature.field_as<double>(colY)));
            }
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

gdx::DenseRaster<double> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const EmissionSector& sector, const RunConfiguration& cfg)
{
    const auto& sectorInv = cfg.sectors();

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(spatialPatternPath);
    auto layer = ds.layer(0);

    const auto gridData = grid_data(GridDefinition::Flanders1km);

    auto colNfrSector  = layer.layer_definition().required_field_index("nfr_sector");
    auto colGnfrSector = layer.layer_definition().field_index("gnfr_sector");
    auto colX          = layer.layer_definition().required_field_index("x_lambert");
    auto colY          = layer.layer_definition().required_field_index("y_lambert");
    auto colEmission   = layer.layer_definition().required_field_index("emission");

    gdx::DenseRaster<double> nfrRaster(gridData.meta, gridData.meta.nodata.value());
    gdx::DenseRaster<double> gnfrRaster(gridData.meta, gridData.meta.nodata.value());

    bool nfrAvailable = false;
    for (const auto& feature : layer) {
        if (auto currentSector = emission_sector_from_feature(feature, colNfrSector, colGnfrSector, sectorInv); currentSector.has_value()) {
            gdx::DenseRaster<double>* rasterPtr = nullptr;
            if (*currentSector == sector) {
                rasterPtr = &nfrRaster;
            } else if (currentSector->type() == EmissionSector::Type::Gnfr && currentSector->gnfr_sector() == sector.gnfr_sector()) {
                rasterPtr = &gnfrRaster;
            }

            if (rasterPtr) {
                const auto cell = cell_for_emission_feature(feature, colX, colY, gridData.meta);
                if (gridData.meta.is_on_map(cell)) {
                    (*rasterPtr)[cell] = feature.field_as<double>(colEmission);
                    if (rasterPtr == &nfrRaster) {
                        nfrAvailable = true;
                    }
                } else {
                    Log::warn("Point outside of flanders extent: {}", Point(feature.field_as<double>(colX), feature.field_as<double>(colY)));
                }
            }
        }
    }

    return nfrAvailable ? std::move(nfrRaster) : std::move(gnfrRaster);
}

static std::string process_ceip_sector(std::string_view str)
{
    if (str::starts_with(str, "N14 ")) {
        return std::string(str.substr(4));
    }

    return std::string(str);
}

gdx::DenseRaster<double> parse_spatial_pattern_ceip(const fs::path& spatialPatternPath, const EmissionIdentifier& id, const RunConfiguration& cfg)
{
    using namespace io;
    CSVReader<8, trim_chars<' ', '\t'>, no_quote_escape<';'>, throw_on_overflow, single_line_comment<'#'>> in(str::from_u8(spatialPatternPath.u8string()));

    const auto& sectors    = cfg.sectors();
    const auto& pollutants = cfg.pollutants();
    const auto& countries  = cfg.countries();

    // ISO2;YEAR;SECTOR;POLLUTANT;LONGITUDE;LATITUDE;UNIT;EMISSION

    int lineNr = 1;

    const auto extent     = grid_data(GridDefinition::ChimereEmep).meta;
    const auto gnfrSector = convert_sector_to_gnfr_level(id.sector);

    gdx::DenseRaster<double> result(extent, extent.nodata.value());

    char *countryStr, *year, *sector, *pollutant, *lonStr, *latStr, *unit, *value;
    while (in.read_row(countryStr, year, sector, pollutant, lonStr, latStr, unit, value)) {
        ++lineNr;

        double emissionValue = to_giga_gram(to_double(value, lineNr), unit);
        auto curPollutant    = pollutants.pollutant_from_string(pollutant);

        if (id.pollutant != curPollutant) {
            continue;
        }

        if (id.country.is_belgium()) {
            // Belgian codes are BEF/BEB/BEW but in the CEIP, they are reported with the BE code
            if (countryStr != "BE") {
                continue;
            }
        } else {
            if (auto country = countries.try_country_from_string(countryStr); country != id.country) {
                continue;
            }
        }

        const auto emissionSector = sectors.sector_from_string(process_ceip_sector(sector));
        bool sectorMatch          = false;
        if (emissionSector.type() == EmissionSector::Type::Gnfr) {
            sectorMatch = id.sector.gnfr_sector() == emissionSector.gnfr_sector();
        } else {
            sectorMatch = id.sector == emissionSector;
        }

        if (sectorMatch) {
            const auto lon = str::to_double(lonStr);
            const auto lat = str::to_double(latStr);

            if (!(lat.has_value() && lon.has_value())) {
                Log::warn("CEIP pattern: invalid lat lon values: lat {} lon {} ({}:{})", latStr, lonStr, spatialPatternPath, lineNr);
                continue;
            }

            const auto cell = extent.convert_point_to_cell(Point(*lon, *lat));
            if (extent.is_on_map(cell)) {
                result.add_to_cell(cell, emissionValue);
            } else {
                Log::warn("CEIP pattern: emission is outside of the grid: lat {} lon {} ({}:{})", *lat, *lon, spatialPatternPath, lineNr);
            }
        }
    }

    return result;
}
}
