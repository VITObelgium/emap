#include "brnoutputbuilder.h"

#include "emap/emissions.h"
#include "infra/cast.h"

namespace emap {

static constexpr const int64_t s_secondsPerYear = 31'536'000;

using namespace inf;

BrnOutputBuilder::BrnOutputBuilder(std::unordered_map<int32_t, SectorParameterConfig> sectorParams,
                                   std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                                   int32_t cellSizeInM)
: _cellSizeInM(cellSizeInM)
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

void BrnOutputBuilder::add_diffuse_output_entry(const EmissionIdentifier& id, int64_t x, int64_t y, double emission)
{
    const auto& sectorParams    = _sectorParams.at(static_cast<int32_t>(id.sector.id()));
    const auto& pollutantParams = _pollutantParams.at(std::string(id.pollutant.code()));

    constexpr double toGramPerYearRatio = 1'000'000.0 / s_secondsPerYear;

    BrnOutputEntry entry;
    entry.x_m   = x;
    entry.y_m   = y;
    entry.q_gs  = emission * toGramPerYearRatio;
    entry.hc_MW = sectorParams.hc_MW;
    entry.h_m   = sectorParams.h_m;
    entry.d_m   = _cellSizeInM;
    entry.s_m   = sectorParams.s_m;
    entry.dv    = truncate<int32_t>(sectorParams.tb);
    entry.cat   = static_cast<int32_t>(id.sector.id());
    entry.area  = static_cast<int32_t>(id.country.id());
    entry.sd    = pollutantParams.sd;
    entry.comp  = id.pollutant.code();
    entry.flow  = 9999.0;
    entry.temp  = 9999.0;

    std::scoped_lock lock(_mutex);
    _diffuseSources[id.pollutant].push_back(entry);
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

void BrnOutputBuilder::write_to_disk(const RunConfiguration& cfg)
{
    if (cfg.grid_definition() == GridDefinition::Vlops1km || cfg.grid_definition() == GridDefinition::Vlops250m) {
        for (const auto& [pol, entries] : _diffuseSources) {
            write_brn_output(entries, cfg.output_path() / create_vlops_output_name(pol, cfg.year(), cfg.output_filename_suffix()));
        }
    }
}

}