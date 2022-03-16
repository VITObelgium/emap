#include "vlopsoutputbuilder.h"

#include "emap/emissions.h"
#include "infra/cast.h"
#include "outputwriters.h"

namespace emap {

static constexpr const int64_t s_secondsPerYear    = 31'536'000;
static constexpr const double s_toGramPerYearRatio = 1'000'000.0 / s_secondsPerYear;

using namespace inf;

VlopsOutputBuilder::VlopsOutputBuilder(std::unordered_map<std::string, SectorParameterConfig> sectorParams,
                                       std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                                       const RunConfiguration& cfg)
: _sectorLevel(cfg.output_sector_level())
, _cfg(cfg)
, _sectorParams(std::move(sectorParams))
, _pollutantParams(std::move(pollutantParams))
{
}

void VlopsOutputBuilder::add_point_output_entry(const EmissionEntry& emission)
{
    assert(emission.coordinate().has_value());
    assert(emission.value().amount().has_value());

    const auto& id              = emission.id();
    const auto& pollutantParams = _pollutantParams.at(std::string(id.pollutant.code()));

    BrnOutputEntry entry;
    entry.x_m   = truncate<int64_t>(emission.coordinate()->x);
    entry.y_m   = truncate<int64_t>(emission.coordinate()->y);
    entry.q_gs  = emission.value().amount().value() * s_toGramPerYearRatio;
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

void VlopsOutputBuilder::add_diffuse_output_entry(const EmissionIdentifier& id, Point<int64_t> loc, double emission, int32_t cellSizeInM)
{
    assert(id.sector.type() == EmissionSector::Type::Nfr);
    std::string mappedSectorName(id.sector.name());
    if (_sectorLevel != SectorLevel::NFR) {
        mappedSectorName = _cfg.sectors().map_nfr_to_output_name(id.sector.nfr_sector());
    }

    std::scoped_lock lock(_mutex);
    auto& current = _diffuseSources[id.pollutant][mappedSectorName][id.country.id()][loc];
    current.value += emission;
    current.cellSize = cellSizeInM;
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

void VlopsOutputBuilder::flush_pollutant(const Pollutant& pol, WriteMode mode)
{
    if (_diffuseSources.size() > 1) {
        throw RuntimeError("Multiple pollutants?");
    }

    if ((!_diffuseSources.empty()) && _diffuseSources.count(pol) != 1) {
        throw RuntimeError("Different pollutant?");
    }

    auto convertMode = [](WriteMode mode) {
        switch (mode) {
        case WriteMode::Create:
            return BrnOutputWriter::OpenMode::Replace;
        case WriteMode::Append:
            return BrnOutputWriter::OpenMode::Append;
        }

        throw std::logic_error("Invalid write mode");
    };

    for (const auto& [pol, sectorValues] : _diffuseSources) {
        const auto& pollutantParams = _pollutantParams.at(std::string(pol.code()));
        std::vector<BrnOutputEntry> entries;

        for (const auto& [sectorName, countryData] : sectorValues) {
            if (sectorName.empty()) {
                continue;
            }

            auto sectorParamsIter = _sectorParams.find(sectorName);
            if (sectorParamsIter == _sectorParams.end()) {
                throw RuntimeError("No sector parameters configured for sector {}", sectorName);
            }

            const auto& sectorParams = sectorParamsIter->second;
            for (const auto& [countryId, locationData] : countryData) {
                for (const auto& [location, entry] : locationData) {
                    BrnOutputEntry brnEntry;
                    brnEntry.x_m   = location.x;
                    brnEntry.y_m   = location.y;
                    brnEntry.q_gs  = entry.value * s_toGramPerYearRatio;
                    brnEntry.hc_MW = sectorParams.hc_MW;
                    brnEntry.h_m   = sectorParams.h_m;
                    brnEntry.d_m   = entry.cellSize;
                    brnEntry.s_m   = sectorParams.s_m;
                    brnEntry.dv    = truncate<int32_t>(sectorParams.tb);
                    brnEntry.cat   = sectorParams.id;
                    brnEntry.area  = static_cast<int32_t>(countryId);
                    brnEntry.sd    = pollutantParams.sd;
                    brnEntry.comp  = pol.code();
                    brnEntry.flow  = 9999.0;
                    brnEntry.temp  = 9999.0;
                    entries.push_back(brnEntry);
                }
            }
        }

        if (_pointSources.count(pol) > 0) {
            append_to_container(entries, _pointSources.at(pol));
        }

        const auto outputPath = _cfg.output_path() / create_vlops_output_name(pol, _cfg.year(), _cfg.output_filename_suffix());
        bool writeHeader      = !fs::exists(outputPath);
        BrnOutputWriter writer(outputPath, convertMode(mode), find_in_map_optional(_outputIndex, pol).value_or(1));
        if (writeHeader) {
            writer.write_header();
        }
        writer.append_entries(entries);
        _outputIndex[pol] = writer.current_index();
    }

    _diffuseSources.clear();
    _pointSources.clear();
}

void VlopsOutputBuilder::flush(WriteMode /*mode*/)
{
    // No final flush needed, everything was flushed in the pollutant stage
}

}