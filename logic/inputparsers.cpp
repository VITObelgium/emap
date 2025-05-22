#include "emap/inputparsers.h"
#include "emap/constants.h"
#include "emap/emissions.h"
#include "emap/griddefinition.h"
#include "emap/inputconversion.h"
#include "emap/runconfiguration.h"
#include "emap/scalingfactors.h"
#include "emap/sector.h"
#include "infra/chrono.h"
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
#include <csv.h>
#include <exception>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emap {

using namespace inf;
namespace gdal = inf::gdal;

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

Range<date::year> parse_year_range(std::string_view yearRange)
{
    using namespace date;

    auto trimmedYear = inf::str::trimmed_view(yearRange);
    if (trimmedYear == "*") {
        return AllYears;
    }

    auto splitted = inf::str::split_view(trimmedYear, '-');
    if (splitted.size() == 1) {
        const auto year = date::year(str::to_uint32_value(yearRange));
        return Range(year, year);
    } else if (splitted.size() == 2) {
        auto year1 = date::year(str::to_uint32_value(splitted[0]));
        auto year2 = date::year(str::to_uint32_value(splitted[1]));
        return Range(year1, year2);
    }

    throw RuntimeError("Invalid year range specification: {}", yearRange);
}

struct PointSourceIdentifier
{
    std::string type;
    std::string scenario;
    chrono::year year;
    Country country;
    EmissionSector sector;
    Pollutant pollutant;
    std::string eilNumber;
    std::string eilPoint;
    std::string subType;
    std::optional<Coordinate> coordinate;
    std::optional<int32_t> dv;
    double height         = 0.0;
    double diameter       = 0.0;
    double temperature    = 0.0;
    double warmthContents = 0.0;
    double flowRate       = 0.0;

#ifdef HAVE_CPP20_CHRONO
    auto operator<=>(const emap::PointSourceIdentifier& other) const = default;
#else
    bool operator==(const emap::PointSourceIdentifier& other) const = default;
#endif

    std::string source_id() const
    {
        return fmt::format("{}_{}_{}_{}_{}_{}_{}_{}", height, diameter, temperature, warmthContents, flowRate, eilPoint, eilNumber, subType);
    }

    EmissionEntry to_emission_entry(double emissionValue) const
    {
        EmissionEntry entry(
            EmissionIdentifier(country, sector, pollutant),
            EmissionValue(emissionValue));

        entry.set_height(height);
        entry.set_diameter(diameter);
        entry.set_temperature(temperature);
        entry.set_warmth_contents(warmthContents);
        entry.set_flow_rate(flowRate);

        entry.set_source_id(source_id());
        entry.set_dv(dv);
        if (coordinate.has_value()) {
            entry.set_coordinate(*coordinate);
        }
        return entry;
    }
};
}

namespace std {
template <>
struct hash<emap::PointSourceIdentifier>
{
    std::size_t operator()(const emap::PointSourceIdentifier& ps) const
    {
        std::size_t seed = 0;
        inf::hash_combine(seed,
                          ps.type,
                          ps.scenario,
                          ps.year,
                          ps.country,
                          ps.sector,
                          ps.pollutant,
                          ps.eilNumber,
                          ps.eilPoint,
                          ps.subType,
                          ps.coordinate,
                          ps.dv,
                          ps.height,
                          ps.diameter,
                          ps.temperature,
                          ps.warmthContents,
                          ps.flowRate);
        return seed;
    }
};
}

namespace emap {

SingleEmissions parse_point_sources(const fs::path& emissionsCsv, const RunConfiguration& cfg)
{
    // pointsource csv columns: type;scenario;year;reporting_country;nfr-sector;pollutant;emission;unit;x;y;hoogte_m;diameter_m;temperatuur_C;warmteinhoud_MW;Debiet_Nm3/u;Type emissie omschrijving;EIL-nummer;Exploitatie naam;NACE-code;EIL Emissiepunt Jaar Naam;Activiteit type;subtype

    const auto& countryInv      = cfg.countries();
    const auto& sectorInv       = cfg.sectors();
    const auto& pollutantInv    = cfg.pollutants();
    const bool combineIdentical = cfg.combine_identical_point_sources();

    size_t lineNr = 2;

    try {
        Log::debug("Parse emissions: {}", emissionsCsv);

        std::vector<EmissionEntry> pointSources;
        std::unordered_map<PointSourceIdentifier, double> pointSourceEmissions;

        using namespace io;
        CSVReader<25, trim_chars<' ', '\t'>, no_quote_escape<';'>, throw_on_overflow> in(file::u8string(emissionsCsv));

        in.read_header(ignore_missing_column | ignore_extra_column,
                       "type",
                       "scenario",
                       "year",
                       "reporting_country",
                       "nfr_sector",
                       "gnfr_sector",
                       "pollutant",
                       "emission",
                       "unit",
                       "x",
                       "y",
                       "hoogte_m",
                       "diameter_m",
                       "temperatuur_C",
                       "warmteinhoud_MW",
                       "debiet_Nm3/u",
                       "Debiet_Nm3/u",
                       "dv",
                       "type_emissie",
                       "EIL_nummer",
                       "exploitatie_naam",
                       "EIL_Emissiepunt_Jaar_Naam",
                       "Activiteit_type",
                       "subtype",
                       "pointsource_index");

        if (!in.has_column("nfr_sector") && !in.has_column("gnfr_sector")) {
            throw RuntimeError("Missing nfr_sector or gnfr_sector column");
        }

        auto sectorType             = in.has_column("nfr_sector") ? EmissionSector::Type::Nfr : EmissionSector::Type::Gnfr;
        bool hasSubTypeCol          = in.has_column("subtype");
        bool hasDvCol               = in.has_column("dv");
        bool hasPointSourceIndexCol = in.has_column("pointsource_index");
        bool hasCoordinates         = in.has_column("x") && in.has_column("y");

        int32_t year = 0, dv = 0;
        double value, height, diameter, temp, warmthContents, flowRate;
        char *type, *scenario, *countryStr, *nfrSectorName, *gnfrSectorName, *pollutantName, *unit, *emissionType, *eilNr, *explName, *eilYearName, *activityType;
        char *subtype = nullptr, *psIndex = nullptr;
        char *x = nullptr, *y = nullptr;
        while (in.read_row(type, scenario, year, countryStr, nfrSectorName, gnfrSectorName, pollutantName, value, unit, x, y, height, diameter, temp, warmthContents, flowRate, flowRate, dv, emissionType, eilNr, explName, eilYearName, activityType, subtype, psIndex)) {
            auto sectorName    = std::string_view(sectorType == EmissionSector::Type::Nfr ? nfrSectorName : gnfrSectorName);
            const auto country = countryInv.try_country_from_string(countryStr);

            if (sectorName.empty() ||
                sectorInv.is_ignored_sector(sectorType, sectorName, *country) ||
                pollutantInv.is_ignored_pollutant(pollutantName, *country)) {
                continue;
            }

            double emissionValue = to_giga_gram(value, unit);
            if (emissionValue == 0.0) {
                continue;
            }

            auto sector    = sectorInv.try_sector_from_string(sectorType, sectorName);
            auto pollutant = pollutantInv.try_pollutant_from_string(pollutantName);

            if (sector.has_value() && pollutant.has_value()) {
                PointSourceIdentifier ps;
                ps.sector         = *sector;
                ps.country        = *country;
                ps.pollutant      = *pollutant;
                ps.height         = height;
                ps.diameter       = diameter;
                ps.temperature    = temp;
                ps.warmthContents = warmthContents;
                ps.flowRate       = flowRate;

                if (hasSubTypeCol) {
                    ps.subType = subtype ? subtype : "none";
                } else if (hasPointSourceIndexCol) {
                    ps.subType = psIndex ? psIndex : "none";
                } else {
                    ps.subType = "none";
                }

                ps.eilNumber = eilNr;
                ps.eilPoint  = eilYearName;

                if (hasCoordinates) {
                    auto xVal = str::to_double(x);
                    auto yVal = str::to_double(y);
                    if (xVal.has_value() && yVal.has_value()) {
                        ps.coordinate = Coordinate(*xVal, *yVal);
                    } else {
                        throw RuntimeError("Invalid coordinate in point sources: x='{}' y='{}'", x, y);
                    }
                }

                if (hasDvCol) {
                    ps.dv = dv;
                }

                if (combineIdentical) {
                    pointSourceEmissions[ps] += emissionValue;
                } else {
                    pointSources.push_back(ps.to_emission_entry(emissionValue));
                }
            } else {
                if (!pollutant.has_value()) {
                    Log::warn("Unknown pollutant name: {}", pollutantName);
                }

                if (!sector.has_value()) {
                    Log::warn("Unknown sector name: {}", sectorName);
                }
            }

            ++lineNr;
        }

        if (combineIdentical) {
            for (const auto& [ps, emission] : pointSourceEmissions) {
                pointSources.push_back(ps.to_emission_entry(emission));
            }
        }

        SingleEmissions result(cfg.year());
        result.set_emissions(std::move(pointSources));
        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} line {} ({})", emissionsCsv, lineNr, e.what());
    }
}

SingleEmissions parse_emissions(EmissionSector::Type sectorType, const fs::path& emissionsCsv, date::year requestYear, const RunConfiguration& cfg, RespectIgnoreList respectIgnores)
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
                if (respectIgnores == RespectIgnoreList::Yes) {
                    if (sectorInv.is_ignored_sector(sectorType, sectorName, *country) || pollutantInv.is_ignored_pollutant(pollutant, *country)) {
                        continue;
                    }
                }

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
            } catch (const std::exception& e) {
                Log::debug("Ignoring line {} in {} ({})", in.get_file_line(), emissionsCsv, e.what());
            }
        }

        return SingleEmissions(requestYear, std::move(entries));
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} ({})", emissionsCsv, e.what());
    }
}

static EmissionSourceType parse_emission_type(std::string_view emissionType)
{
    auto trimmed = str::trimmed_view(emissionType);

    if (str::iequals(trimmed, "point")) {
        return EmissionSourceType::Point;
    }

    if (str::iequals(trimmed, "diffuse")) {
        return EmissionSourceType::Diffuse;
    }

    if (trimmed == "*") {
        return EmissionSourceType::Any;
    }

    throw RuntimeError("Invalid emission type: {}", emissionType);
}

ScalingFactors parse_scaling_factors(const fs::path& scalingFactors, const RunConfiguration& cfg)
{
    // csv columns: country;nfr_sector;pollutant;factor

    const auto& countryInv   = cfg.countries();
    const auto& sectorInv    = cfg.sectors();
    const auto& pollutantInv = cfg.pollutants();
    size_t lineNr            = 2;

    try {
        Log::debug("Parse scaling factors: {}", scalingFactors);

        ScalingFactors result;

        auto ds    = gdal::VectorDataSet::open(scalingFactors);
        auto layer = ds.layer("Scaling");

        auto colYear         = layer.required_field_index("year");
        auto colEmissionType = layer.required_field_index("emission_type");
        auto colPollutant    = layer.required_field_index("pollutant_code");
        auto colCountry      = layer.required_field_index("country_iso_code");
        auto colGnfr         = layer.required_field_index("GNFR_code");
        auto colNfr          = layer.required_field_index("NFR_code");
        auto colScaleFactor  = layer.required_field_index("scale_factor");

        for (auto& feature : layer) {
            const auto year = feature.field_as<std::string>(colYear);
            if (year.empty()) {
                // skip empty lines
                continue;
            }

            // Empty optional values mean match any (*)
            std::optional<Country> country;
            std::optional<Pollutant> pollutant;
            std::optional<NfrSector> nfrSector;
            std::optional<GnfrSector> gnfrSector;
            if (auto sec = sectorInv.try_sector_from_string(EmissionSector::Type::Nfr, feature.field_as<std::string_view>(colNfr)); sec.has_value()) {
                nfrSector = sec->nfr_sector();
            }

            if (auto sec = sectorInv.try_sector_from_string(EmissionSector::Type::Gnfr, feature.field_as<std::string_view>(colGnfr)); sec.has_value()) {
                gnfrSector = sec->gnfr_sector();
            }

            if (!nfrSector.has_value()) {
                if (auto name = feature.field_as<std::string_view>(colNfr); name != "*") {
                    throw RuntimeError("Invalid NFR sector: {}", name);
                }
            }

            if (!gnfrSector.has_value()) {
                if (auto name = feature.field_as<std::string_view>(colGnfr); name != "*") {
                    throw RuntimeError("Invalid GNFR sector: {}", name);
                }
            }

            if (nfrSector.has_value() && gnfrSector.has_value() && (EmissionSector(*nfrSector).gnfr_sector() != *gnfrSector)) {
                throw RuntimeError("GNFR sector column does not match with the NFR sector column: {} <-> {}", feature.field_as<std::string_view>(colNfr), feature.field_as<std::string_view>(colGnfr));
            }

            if (auto cnt = countryInv.try_country_from_string(feature.field_as<std::string_view>(colCountry)); cnt.has_value()) {
                country = *cnt;
            } else {
                if (str::trimmed_view(feature.field_as<std::string_view>(colCountry)) != "*") {
                    throw RuntimeError("Invalid country code: {}", feature.field_as<std::string_view>(colCountry));
                }
            }

            if (auto pol = pollutantInv.try_pollutant_from_string(feature.field_as<std::string_view>(colPollutant)); pol.has_value()) {
                if (pol->code() == constants::pollutant::PMCoarse) {
                    throw RuntimeError("PMCoarse is not allowed to be scaled");
                }
                pollutant = *pol;
            } else {
                if (str::trimmed_view(feature.field_as<std::string_view>(colPollutant)) != "*") {
                    throw RuntimeError("Invalid pollutant code: {}", feature.field_as<std::string_view>(colCountry));
                }
            }

            const auto emissionType = parse_emission_type(feature.field_as<std::string_view>(colEmissionType));
            const auto factor       = feature.field_as<double>(colScaleFactor);

            if (factor != 1.0) {
                result.add_scaling_factor(ScalingFactor(country, nfrSector, gnfrSector, pollutant, emissionType, parse_year_range(year), factor));
            }

            ++lineNr;
        }

        return result;
    } catch (const std::exception& e) {
        throw RuntimeError("Error parsing {} (line {}: {})", scalingFactors, lineNr, e.what());
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

static std::optional<Pollutant> detect_pollutant_name_from_header(std::string_view hdr, const PollutantInventory& pollutantInv, const Country& country)
{
    std::optional<Pollutant> pol;

    try {
        if (!pollutantInv.is_ignored_pollutant(hdr, country)) {
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
    Log::debug("Parse emissions belgium: {}", emissionsData);

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
                if (auto pol = detect_pollutant_name_from_header(strip_newline(feature.field_as<std::string_view>(i)), pollutantInv, country); pol.has_value()) {
                    pollutantColumns.emplace(i, *pol);
                }
            }
        } else if (lineNr == unitLineNr) {
            for (auto& [index, pol] : pollutantColumns) {
                pol.unitConversion = to_giga_gram_factor(feature.field_as<std::string_view>(index)).value_or(1.0);
            }
        }

        if (auto nfrSectorName = feature.field_as<std::string_view>(1); !nfrSectorName.empty()) {
            if (sectorInv.is_ignored_nfr_sector(nfrSectorName, country)) {
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

            for (const auto& [index, polData] : pollutantColumns) {
                const auto field = feature.field(index);
                std::optional<double> emissionValue;
                if (const auto* emission = std::get_if<double>(&field)) {
                    emissionValue = (*emission) * polData.unitConversion;
                } else if (const auto* emission = std::get_if<std::string_view>(&field)) {
                    emissionValue = parse_emission_value(*emission);
                    if (!emissionValue.has_value()) {
                        if (!sectorOverride) {
                            emissionValue = 0.0;
                        }
                    } else {
                        emissionValue = (*emissionValue) * polData.unitConversion;
                    }
                }

                if (emissionValue.has_value()) {
                    if (sectorOverride) {
                        // update the existing emission with the higher priority version
                        update_entry(entries, EmissionEntry(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue)));
                    } else {
                        entries.emplace_back(EmissionIdentifier(country, nfrSector, polData.pollutant), EmissionValue(*emissionValue));
                    }
                } else if (!sectorOverride) {
                    const auto value = feature.field_as<std::string>(index);
                    if (!value.empty()) {
                        Log::error("Failed to obtain emission value from {}", value);
                    }
                }
            }
        }
    }

    return SingleEmissions(year, entries);
}

static std::optional<EmissionSector> emission_sector_from_feature(const gdal::Feature& feature, int colNfr, int colGnfr, const Country& country, const SectorInventory& sectorInv)
{
    try {
        std::string nfrSectorName(str::trimmed_view(feature.field_as<std::string_view>(colNfr)));
        if (!nfrSectorName.empty()) {
            // Nfr sector
            if (sectorInv.is_ignored_nfr_sector(nfrSectorName, country)) {
                return {};
            }

            return EmissionSector(sectorInv.nfr_sector_from_string(nfrSectorName));
        } else if (colGnfr >= 0) {
            // Gnfr sector
            std::string gnfrSectorName(str::trimmed_view(feature.field_as<std::string_view>(colGnfr)));
            if (sectorInv.is_ignored_gnfr_sector(gnfrSectorName, country)) {
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

        if (auto sector = emission_sector_from_feature(feature, colNfrSector, colGnfrSector, id.country, sectorInv); sector.has_value()) {
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
        if (auto currentSector = emission_sector_from_feature(feature, colNfrSector, colGnfrSector, country::BEF, sectorInv); currentSector.has_value()) {
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

    bool isBelgium = id.country.is_belgium();

    char *countryStr, *year, *sector, *pollutant, *lonStr, *latStr, *unit, *value;
    while (in.read_row(countryStr, year, sector, pollutant, lonStr, latStr, unit, value)) {
        ++lineNr;

        double emissionValue = to_giga_gram(to_double(value, lineNr), unit);
        auto curPollutant    = pollutants.pollutant_from_string(pollutant);

        if (id.pollutant != curPollutant) {
            continue;
        }

        if (isBelgium) {
            // Belgian codes are BEF/BEB/BEW but in the CEIP, they are reported with the BE code
            if (std::string_view(countryStr) != "BE") {
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
