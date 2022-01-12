#include "emap/inputparsers.h"
#include "emap/constants.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
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

static Country to_country(std::string_view name)
{
    return Country::from_string(name);
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

SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
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
                EmissionIdentifier(to_country(line.get_string(colCountry)), sectorInv.sector_from_string(sectorType, line.get_string(colSector)), pollutantInv.pollutant_from_string(line.get_string(colPollutant))),
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

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
{
    // First lines are comments
    // Format: ISO2;YEAR;SECTOR;POLLUTANT;UNIT;NUMBER/FLAG

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

            const auto country = Country::try_from_string(countryStr);
            if (!country.has_value() || !country->included()) {
                // not interested in this country, no need to report this
                continue;
            }

            try {
                EmissionEntry info(
                    EmissionIdentifier(*country, sectorInv.sector_from_string(sectorType, sector), pollutantInv.pollutant_from_string(pollutant)), EmissionValue(emissionValue));

                result.add_emission(std::move(info));
            } catch (const std::exception& e) {
                Log::debug("Ignoring line {} in {} ({})", in.get_file_line(), emissionsCsv, e.what());
            }
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", emissionsCsv, e.what());
    }
}

ScalingFactors parse_scaling_factors(const fs::path& scalingFactorsCsv, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
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

SingleEmissions parse_point_sources_flanders(const fs::path& emissionsData, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
{
    SingleEmissions result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(emissionsData);
    auto layer = ds.layer(0);

    auto colX           = layer.layer_definition().required_field_index("Emissiepunt X Coördinaat (Lambert)");
    auto colY           = layer.layer_definition().required_field_index("Emissiepunt Y Coördinaat (Lambert)");
    auto colEmission    = layer.layer_definition().required_field_index("Emissie");
    auto colUnit        = layer.layer_definition().required_field_index("Eenheid emissie");
    auto colHeight      = layer.layer_definition().required_field_index("Emissiepunt Hoogte      (m)");
    auto colDiameter    = layer.layer_definition().required_field_index("Emissiepunt Diameter (m)");
    auto colTemperature = layer.layer_definition().required_field_index("Temperatuur (°C)");
    auto colWarmth      = layer.layer_definition().required_field_index("Warmte inhoud    (MW)");
    auto colFlowRate    = layer.layer_definition().required_field_index("Debiet");

    auto colSector    = layer.layer_definition().required_field_index("IPCC NFR Code");
    auto colPollutant = layer.layer_definition().required_field_index("Verontreinigende stof");

    /*struct PmPointEmissionId
    {
        bool operator==(const PmPointEmissionId& other) const noexcept
        {
            return sector == other.sector &&
                   coordinate == other.coordinate &&
                   height == other.height &&
                   diameter == other.diameter &&
                   temperature == other.temperature &&
                   warmthContents == other.warmthContents &&
                   flowRate == other.flowRate;
        }

        EmissionSector sector;
        std::optional<Coordinate> coordinate;
        double height         = 0.0;
        double diameter       = 0.0;
        double temperature    = 0.0;
        double warmthContents = 0.0;
        double flowRate       = 0.0;
    };

    struct PmPointEmissionValue
    {
        PmPointEmissionValue() = default;
        PmPointEmissionValue(PmPointEmissionId id_)
        : id(id_)
        {
        }

        PmPointEmissionId id;
        std::optional<EmissionEntry> pm10;
        std::optional<EmissionEntry> pm25;
    };

    std::vector<PmPointEmissionValue> pmEmissions;*/

    for (auto& feature : layer) {
        if (feature.field_as<std::string_view>(colPollutant).empty()) {
            continue; // skip empty rows
        }

        EmissionEntry info(
            EmissionIdentifier(Country::Id::BEF,
                               sectorInv.sector_from_string(EmissionSector::Type::Nfr, feature.field_as<std::string_view>(colSector)),
                               pollutantInv.pollutant_from_string(feature.field_as<std::string_view>(colPollutant))),
            EmissionValue(to_giga_gram(feature.field_as<double>(colEmission), feature.field_as<std::string_view>(colUnit))));

        info.set_coordinate(Coordinate(feature.field_as<int32_t>(colX), feature.field_as<int32_t>(colY)));
        info.set_height(feature.field_as<double>(colHeight));
        info.set_diameter(feature.field_as<double>(colDiameter));
        info.set_temperature(feature.field_as<double>(colTemperature));
        info.set_warmth_contents(feature.field_as<double>(colWarmth));
        info.set_flow_rate(feature.field_as<double>(colFlowRate));

        /*if (info.pollutant() == Pollutant::Id::PM10 || info.pollutant() == Pollutant::Id::PM2_5) {
            PmPointEmissionId pointId;
            pointId.sector         = info.id().sector;
            pointId.coordinate     = info.coordinate();
            pointId.height         = info.height();
            pointId.diameter       = info.diameter();
            pointId.temperature    = info.temperature();
            pointId.warmthContents = info.warmth_contents();
            pointId.flowRate       = info.flow_rate();

            bool inserted = false;

            auto* iter = find_in_container(pmEmissions, [&pointId](const PmPointEmissionValue& val) {
                return val.id == pointId;
            });

            if (iter) {
                if (info.pollutant() == Pollutant::Id::PM10) {
                    if (iter->pm10.has_value()) {
                        throw RuntimeError("Point emission id pm10 unique check failed");
                    }

                    iter->pm10 = info;
                } else {
                    if (iter->pm25.has_value()) {
                        throw RuntimeError("Point emission id pm2.5 unique check failed");
                    }

                    iter->pm25 = info;
                }
            }
        }*/

        result.add_emission(std::move(info));
    }

    //// Add PmCoarse entries if Pm10 and Pm2.5 are available
    //for (auto& emission : pmEmissions) {
    //    if (emission.pm10.has_value() && emission.pm25.has_value()) {
    //        if (emission.pm10->value().amount() < emission.pm25->value().amount()) {
    //            throw RuntimeError("Invalid PM data for point source id (Sector {} (PM10: {}, PM2.5 {})", emission.id.sector, emission.pm10->value().amount(), emission.pm25->value().amount());
    //        }

    //        EmissionEntry pmc(
    //            EmissionIdentifier(Country::Id::BEF, emission.id.sector, Pollutant::Id::PMcoarse),
    //            EmissionValue(EmissionValue(emission.pm10->value().amount() - emission.pm25->value().amount())));

    //        if (emission.id.coordinate.has_value()) {
    //            pmc.set_coordinate(*emission.id.coordinate);
    //        }

    //        pmc.set_height(emission.pm10->height());
    //        pmc.set_diameter(emission.pm10->diameter());
    //        pmc.set_temperature(emission.pm10->temperature());
    //        pmc.set_warmth_contents(emission.pm10->warmth_contents());
    //        pmc.set_flow_rate(emission.pm10->flow_rate());

    //        result.add_emission(std::move(pmc));
    //    }
    //}

    return result;
}

static Country detect_belgian_region_from_filename(const fs::path& path)
{
    auto filename = path.stem().u8string();
    if (str::starts_with(filename, "BEB")) {
        return Country(Country::Id::BEB); // Brussels
    } else if (str::starts_with(filename, "BEF")) {
        return Country(Country::Id::BEF); // Flanders
    } else if (str::starts_with(filename, "BEW")) {
        return Country(Country::Id::BEW); // Wallonia
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

SingleEmissions parse_emissions_belgium(const fs::path& emissionsData, date::year year, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
{
    SingleEmissions result;

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

    int lineNr = 0;
    for (const auto& feature : layer) {
        ++lineNr;

        if (lineNr == pollutantLineNr) {
            for (int i = 0; i < feature.field_count(); ++i) {
                if (auto pol = detect_pollutant_name_from_header(feature.field_as<std::string_view>(i), pollutantInv); pol.has_value()) {
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

            // TODO: Sector overrides
            std::optional<NfrSector> sectorOverride;

            try {
                nfrSector = EmissionSector(sectorInv.nfr_sector_from_string(nfrSectorName));
                //sectorOverride = nfrSector.is_sector_override();
                if (sectorOverride.has_value()) {
                    // this sector overrides the value of another sector
                    nfrSector = EmissionSector(*sectorOverride);
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

                    if (sectorOverride.has_value()) {
                        // update the existing emission with the override
                        result.update_or_add_emission(EmissionEntry(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue)));
                    } else {
                        result.add_emission(EmissionEntry(
                            EmissionIdentifier(country, nfrSector, polData.pollutant),
                            EmissionValue(*emissionValue)));
                    }
                } else {
                    const auto value = feature.field_as<std::string>(index);
                    if (!value.empty() && value != "NO" && value != "IE" && value != "NA" && value != "NE" && value != "NR") {
                        Log::error("Failed to obtain emission value from {}", value);
                    }
                }
            }

            if (const auto pmCoarse = pollutantInv.try_pollutant_from_string(constants::pollutant::PMCoarse); pmCoarse.has_value()) {
                // This config has a PMCoarse pollutant, add it as difference of PM10 and PM2.5

                if (pm2_5.has_value() && pm10.has_value()) {
                    if (pm10 >= pm2_5) {
                        auto pmcVal = EmissionValue(*pm10 - *pm2_5);
                        if (sectorOverride.has_value()) {
                            // update the existing emission with the override
                            result.update_or_add_emission(EmissionEntry(EmissionIdentifier(country, nfrSector, *pmCoarse), pmcVal));
                        } else {
                            result.add_emission(EmissionEntry(
                                EmissionIdentifier(country, nfrSector, *pmCoarse),
                                pmcVal));
                        }
                    }
                } else {
                    throw RuntimeError("Invalid PM data for sector {} (PM10: {}, PM2.5 {})", nfrSector, *pm10, *pm2_5);
                }
            }
        }
    }

    return result;
}

std::vector<SpatialPatternData> parse_spatial_pattern_flanders(const fs::path& spatialPatternPath, const SectorInventory& sectorInv, const PollutantInventory& pollutantInv)
{
    std::vector<SpatialPatternData> result;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds    = gdal::VectorDataSet::open(spatialPatternPath);
    auto layer = ds.layer(0);
    std::optional<date::year> year;

    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    const auto gridData  = grid_data(GridDefinition::Flanders1km);

    EmissionIdentifier id;
    id.country = Country::Id::BEF;

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

SectorInventory parse_sectors(const fs::path& sectorSpec)
{
    std::vector<GnfrSector> gnfrSectors;
    std::vector<NfrSector> nfrSectors;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(sectorSpec);

    {
        auto layer = ds.layer("GNFR");

        const auto colNumber = layer.layer_definition().required_field_index("GNFR_number");
        const auto colLabel  = layer.layer_definition().required_field_index("GNFR_label");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            // TODO: destination
            gnfrSectors.emplace_back(feature.field_as<std::string_view>(colLabel), GnfrId(feature.field_as<int64_t>(colNumber)), "", EmissionDestination::Land);
        }
    }

    {
        auto layer = ds.layer("NFR");

        const auto colCode        = layer.layer_definition().required_field_index("NFR_code");
        const auto colNumber      = layer.layer_definition().required_field_index("NFR_number");
        const auto colDescription = layer.layer_definition().required_field_index("NFR_description");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            // TODO find gnfr sector based on config
            /*auto gnfrSector = find_in_container(gnfrSectors, [gnfrName = feature.field_as<std::string_view>(colGnfr)](const GnfrSector& sector) {
                return sector.name() == gnfrName;
            })*/

            nfrSectors.emplace_back(feature.field_as<std::string_view>(colCode), NfrId(feature.field_as<int64_t>(colNumber)), GnfrSector(), feature.field_as<std::string_view>(colDescription));
        }
    }

    return SectorInventory(std::move(gnfrSectors), std::move(nfrSectors));
}

PollutantInventory parse_pollutants(const fs::path& pollutantSpec)
{
    std::vector<Pollutant> pollutants;

    CPLSetThreadLocalConfigOption("OGR_XLSX_HEADERS", "FORCE");
    auto ds = gdal::VectorDataSet::open(pollutantSpec);

    {
        auto layer = ds.layer("pollutant");

        const auto colCode  = layer.layer_definition().required_field_index("pollutant_code");
        const auto colLabel = layer.layer_definition().required_field_index("pollutant_label");

        for (const auto& feature : layer) {
            if (!feature.field_is_valid(0)) {
                continue; // skip empty lines
            }

            pollutants.emplace_back(feature.field_as<std::string_view>(colCode), feature.field_as<std::string_view>(colLabel));
        }
    }

    return PollutantInventory(std::move(pollutants));
}

}
