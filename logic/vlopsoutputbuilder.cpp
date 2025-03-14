#include "vlopsoutputbuilder.h"

#include "emap/constants.h"
#include "emap/emissions.h"
#include "infra/cast.h"
#include "infra/log.h"
#include "outputwriters.h"

namespace emap {

using namespace inf;

VlopsOutputBuilder::VlopsOutputBuilder(SectorParameterConfiguration sectorParams,
                                       std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                                       const RunConfiguration& cfg)
: _sectorLevel(cfg.output_sector_level())
, _cfg(cfg)
, _sectorParams(std::move(sectorParams))
, _pollutantParams(std::move(pollutantParams))
{
}

static std::string_view vlops_pollutant_name(const Pollutant& pol)
{
    if (pol.code() == "PMcoarse") {
        return "PMc";
    } else if (pol.code() == "PCDD-PCDF") {
        return "DIX";
    } else if (pol.code() == "Indeno") {
        return "Ind";
    }

    return pol.code().substr(0, 5);
}

void VlopsOutputBuilder::add_point_output_entry(const EmissionEntry& emission)
{
    assert(emission.coordinate().has_value());
    assert(emission.value().amount().has_value());

    const auto& id              = emission.id();
    const auto& pollutantParams = _pollutantParams.at(std::string(id.pollutant.code()));

    const auto mappedSectorName = _cfg.sectors().map_nfr_to_output_name(id.sector.nfr_sector());
    auto sectorParams           = _sectorParams.get_parameters(mappedSectorName, id.pollutant);

    BrnOutputEntry entry;
    entry.ssn   = static_cast<int>(_cfg.year());
    entry.x_m   = truncate<int64_t>(emission.coordinate()->x);
    entry.y_m   = truncate<int64_t>(emission.coordinate()->y);
    entry.q_gs  = emission.value().amount().value() * constants::toGramPerYearRatio;
    entry.hc_MW = emission.warmth_contents();
    entry.h_m   = emission.height();
    entry.d_m   = 0;
    entry.s_m   = 0;
    entry.dv    = emission.dv().value_or(1);
    entry.cat   = sectorParams.id;
    entry.area  = static_cast<int32_t>(id.country.id());
    entry.sd    = pollutantParams.sd;
    entry.comp  = vlops_pollutant_name(id.pollutant);
    entry.flow  = emission.flow_rate();
    entry.temp  = emission.temperature();

    std::scoped_lock lock(_mutex);
    _pointSources[id.pollutant].push_back(entry);
}

void VlopsOutputBuilder::add_diffuse_output_entry(const EmissionIdentifier& id, Point<double> loc, double emission, int32_t cellSizeInM)
{
    assert(id.sector.type() == EmissionSector::Type::Nfr);
    const auto mappedSectorName = _cfg.sectors().map_nfr_to_output_name(id.sector.nfr_sector());

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

    return file::u8path(filename);
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

            auto sectorParams = _sectorParams.get_parameters(sectorName, pol);

            for (const auto& [countryId, locationData] : countryData) {
                for (const auto& [location, entry] : locationData) {
                    BrnOutputEntry brnEntry;
                    brnEntry.ssn   = static_cast<int>(_cfg.year());
                    brnEntry.x_m   = truncate<int64_t>(location.x);
                    brnEntry.y_m   = truncate<int64_t>(location.y);
                    brnEntry.q_gs  = entry.value * constants::toGramPerYearRatio;
                    brnEntry.hc_MW = sectorParams.hc_MW;
                    brnEntry.h_m   = sectorParams.h_m;
                    brnEntry.d_m   = entry.cellSize;
                    brnEntry.s_m   = sectorParams.s_m;
                    brnEntry.dv    = truncate<int32_t>(sectorParams.tb);
                    brnEntry.cat   = sectorParams.id;
                    brnEntry.area  = static_cast<int32_t>(countryId);
                    brnEntry.sd    = pollutantParams.sd;
                    brnEntry.comp  = vlops_pollutant_name(pol);
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
        BrnOutputWriter writer(outputPath, convertMode(mode));
        if (writeHeader) {
            writer.write_header();
        }
        writer.append_entries(entries);
    }

    _diffuseSources.clear();
    _pointSources.clear();
}

void VlopsOutputBuilder::flush(WriteMode /*mode*/)
{
    // No final flush needed, everything was flushed in the pollutant stage
}

}
