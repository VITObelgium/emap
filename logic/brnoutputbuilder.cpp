#include "brnoutputbuilder.h"

#include "emap/emissions.h"
#include "infra/cast.h"
#include "outputwriters.h"

namespace emap {

static constexpr const int64_t s_secondsPerYear = 31'536'000;

using namespace inf;

BrnOutputBuilder::BrnOutputBuilder(std::unordered_map<std::string, SectorParameterConfig> sectorParams,
                                   std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                                   SectorLevel sectorLevel)
: _sectorLevel(sectorLevel)
, _sectorParams(std::move(sectorParams))
, _pollutantParams(std::move(pollutantParams))
{
}

void BrnOutputBuilder::add_point_output_entry(const EmissionEntry& emission)
{
    assert(emission.coordinate().has_value());
    assert(emission.value().amount().has_value());

    const auto& id              = emission.id();
    const auto& pollutantParams = _pollutantParams.at(std::string(id.pollutant.code()));

    constexpr double toGramPerYearRatio = 1'000'000.0 / s_secondsPerYear;

    BrnOutputEntry entry;
    entry.x_m   = emission.coordinate()->x;
    entry.y_m   = emission.coordinate()->y;
    entry.q_gs  = emission.value().amount().value() * toGramPerYearRatio;
    entry.hc_MW = emission.warmth_contents();
    entry.h_m   = emission.height();
    entry.d_m   = 0;
    entry.s_m   = 0;
    entry.dv    = 1;
    entry.cat   = static_cast<int32_t>(id.sector.id());
    entry.area  = static_cast<int32_t>(id.country.id());
    entry.sd    = pollutantParams.sd;
    entry.comp  = id.pollutant.code();
    entry.flow  = emission.flow_rate();
    entry.temp  = emission.temperature();

    std::scoped_lock lock(_mutex);
    _pointSources[id.pollutant].push_back(entry);
}

void BrnOutputBuilder::add_diffuse_output_entry(const EmissionIdentifier& id, Point<int64_t> loc, double emission, int32_t cellSizeInM)
{
    // TODO: support custom sector mappings
    std::scoped_lock lock(_mutex);
    auto& current = _diffuseSources[id.pollutant][output_level_name(id.sector)][id.country.id()][loc];
    if (current.value > 0.0) {
        assert(current.cellSize == cellSizeInM;);
    }
    current.value += emission;
    current.cellSize                              = cellSizeInM;
    _sectorIdLookup[output_level_name(id.sector)] = id.sector.id();
}

static fs::path create_vlops_output_name(const Pollutant& pol, date::year year, std::string_view suffix)
{
    auto filename = fmt::format("{}_OPS_{}", pol.code(), static_cast<int32_t>(year));
    if (!suffix.empty()) {
        filename += suffix;
    }
    filename += ".brn";

    return fs::u8path(filename);
}

void BrnOutputBuilder::write_to_disk(const RunConfiguration& cfg, WriteMode mode)
{
    constexpr double toGramPerYearRatio = 1'000'000.0 / s_secondsPerYear;

    auto convertMode = [](WriteMode mode) {
        switch (mode) {
        case WriteMode::Create:
            return BrnOutputWriter::OpenMode::Replace;
        case WriteMode::Append:
            return BrnOutputWriter::OpenMode::Append;
        }

        throw std::logic_error("Invalid write mode");
    };

    if (cfg.model_grid() == ModelGrid::Vlops1km ||
        cfg.model_grid() == ModelGrid::Vlops250m) {
        for (const auto& [pol, sectorValues] : _diffuseSources) {
            const auto& pollutantParams = _pollutantParams.at(std::string(pol.code()));
            std::vector<BrnOutputEntry> entries;

            for (const auto& [sectorName, countryData] : sectorValues) {
                const auto& sectorParams = _sectorParams.at(sectorName);
                for (const auto& [countryId, locationData] : countryData) {
                    for (const auto& [location, entry] : locationData) {
                        BrnOutputEntry brnEntry;
                        brnEntry.x_m   = location.x;
                        brnEntry.y_m   = location.y;
                        brnEntry.q_gs  = entry.value * toGramPerYearRatio;
                        brnEntry.hc_MW = sectorParams.hc_MW;
                        brnEntry.h_m   = sectorParams.h_m;
                        brnEntry.d_m   = entry.cellSize;
                        brnEntry.s_m   = sectorParams.s_m;
                        brnEntry.dv    = truncate<int32_t>(sectorParams.tb);
                        brnEntry.cat   = _sectorIdLookup.at(sectorName);
                        brnEntry.area  = static_cast<int32_t>(countryId);
                        brnEntry.sd    = pollutantParams.sd;
                        brnEntry.comp  = pol.code();
                        brnEntry.flow  = 9999.0;
                        brnEntry.temp  = 9999.0;
                        entries.push_back(brnEntry);
                    }
                }
            }

            BrnOutputWriter writer(cfg.output_path() / create_vlops_output_name(pol, cfg.year(), cfg.output_filename_suffix()), convertMode(mode));
            if (mode == WriteMode::Create) {
                writer.write_header();
            }
            writer.append_entries(entries);
        }
    }
}

std::string BrnOutputBuilder::output_level_name(const EmissionSector& sector) const
{
    assert(sector.type() == EmissionSector::Type::Nfr);

    switch (_sectorLevel) {
    case SectorLevel::GNFR:
        return std::string(sector.gnfr_name());
    case SectorLevel::NFR:
        return std::string(sector.name());
    case SectorLevel::Custom:
        throw RuntimeError("Custom levels not implemented");
    }

    return std::string();
}
}