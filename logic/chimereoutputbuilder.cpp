#include "chimereoutputbuilder.h"

#include "emap/emissions.h"
#include "infra/cast.h"
#include "infra/conversion.h"
#include "outputwriters.h"

namespace emap {

using namespace inf;

ChimereOutputBuilder::ChimereOutputBuilder(SectorParameterConfiguration sectorParams,
                                           std::unordered_map<CountryId, int32_t> countryMapping,
                                           const RunConfiguration& cfg)
: _sectorLevel(cfg.output_sector_level())
, _cfg(cfg)
, _meta(grid_data(grids_for_model_grid(cfg.model_grid()).front()).meta)
, _countryMapping(std::move(countryMapping))
, _sectorParams(std::move(sectorParams))
{
    size_t index = 0;
    for (auto& pol : cfg.included_pollutants()) {
        _pollutantIndexes.emplace(pol, index++);
    }

    index = 0;
    for (const auto& name : _sectorParams.sector_names_sorted_by_id()) {
        _sectorIndexes[name] = index++;
    }
}

Cell ChimereOutputBuilder::coordinate_to_chimere_cell(const inf::Point<double>& point) const
{
    Cell gridCell = _meta.convert_point_to_cell(point);
    return Cell(_meta.rows - gridCell.r, gridCell.c + 1);
}

void ChimereOutputBuilder::add_point_output_entry(const EmissionEntry& emission)
{
    assert(emission.coordinate().has_value());
    assert(emission.value().amount().has_value());

    const auto& id = emission.id();

    if (!_meta.is_on_map(emission.coordinate().value())) {
        return;
    }

    const auto sectorName = _cfg.sectors().map_nfr_to_output_name(emission.id().sector.nfr_sector());

    if (_cfg.output_point_sources_separately()) {
        DatPointSourceOutputEntry entry;
        entry.coordinate  = to_coordinate(emission.coordinate().value());
        entry.countryCode = _countryMapping.at(id.country.id());
        entry.sectorId    = _sectorParams.get_parameters(sectorName, id.pollutant).id;
        entry.temperature = emission.temperature();
        entry.velocity    = 0.0;
        entry.height      = emission.height();
        entry.diameter    = emission.diameter();
        entry.emissions.resize(_pollutantIndexes.size(), 0.0);
        entry.emissions[_pollutantIndexes.at(id.pollutant)] = emission.value().amount().value_or(0.0) * 1000.0;

        std::scoped_lock lock(_mutex);
        _pointSources.push_back(entry);
    } else {
        const auto countryCode = _countryMapping.at(id.country.id());
        const auto cell        = coordinate_to_chimere_cell(emission.coordinate().value());
        const auto sectorName  = _cfg.sectors().map_nfr_to_output_name(emission.id().sector.nfr_sector());

        std::scoped_lock lock(_mutex);
        _diffuseSources[id.pollutant][countryCode][cell][sectorName] += emission.value().amount().value_or(0.0) * 1000.0;
    }
}

void ChimereOutputBuilder::add_diffuse_output_entry(const EmissionIdentifier& id, Point<double> loc, double emission, int32_t /*cellSizeInM*/)
{
    if (!_meta.is_on_map(loc)) {
        return;
    }

    assert(id.sector.type() == EmissionSector::Type::Nfr);
    const auto mappedSectorName = _cfg.sectors().map_nfr_to_output_name(id.sector.nfr_sector());
    const auto mappedCountry    = _countryMapping.at(id.country.id());
    const auto chimereCell      = coordinate_to_chimere_cell(loc);

    std::scoped_lock lock(_mutex);
    _diffuseSources[id.pollutant][mappedCountry][chimereCell][mappedSectorName] += emission * 1000.0;
}

static std::string_view grid_resolution_string(ModelGrid grid)
{
    switch (grid) {
    case ModelGrid::Chimere05deg:
        return "05deg";
    case ModelGrid::Chimere01deg:
        return "01deg";
    case ModelGrid::Chimere005degLarge:
        return "005deg_large";
    case ModelGrid::Chimere005degSmall:
        return "005deg_small";
    case ModelGrid::Chimere0025deg:
        return "0025deg";
    case ModelGrid::ChimereEmep:
    case ModelGrid::SherpaEmep:
        return "emep_01deg";
    case ModelGrid::ChimereCams:
        return "cams_01-005deg";
    case ModelGrid::ChimereRio1:
        return "chimere_rio1";
    case ModelGrid::ChimereRio4:
        return "chimere_rio4";
    case ModelGrid::ChimereRio32:
        return "chimere_rio32";
    case ModelGrid::EnumCount:
    case ModelGrid::Invalid:
        break;
    }

    throw RuntimeError("Invalid chimere model grid");
}

static fs::path create_chimere_output_name(ModelGrid grid, const Pollutant& pol, date::year year, std::string_view suffix)
{
    // output_Chimere_resolutie_polluent_zichtjaar_suffix
    return file::u8path(fmt::format("output_Chimere_{}_{}_{}{}.dat", grid_resolution_string(grid), pol.code(), static_cast<int32_t>(year), suffix));
}

static fs::path create_chimere_point_source_output_name(date::year year, std::string_view suffix)
{
    // output_Chimere_pointsources_zichtjaar_suffix_ps
    return file::u8path(fmt::format("output_Chimere_pointsources_{}{}_ps.dat", static_cast<int32_t>(year), suffix));
}

void ChimereOutputBuilder::flush_pollutant(const Pollutant& pol, WriteMode /*mode*/)
{
    if (_diffuseSources.size() > 1) {
        throw RuntimeError("Multiple pollutants?");
    }

    if ((!_diffuseSources.empty()) && _diffuseSources.count(pol) != 1) {
        throw RuntimeError("Different pollutant?");
    }

    for (const auto& [pol, countryData] : _diffuseSources) {
        std::vector<DatOutputEntry> entries;

        for (const auto& [countryCode, cellData] : countryData) {
            for (const auto& [cell, sectorData] : cellData) {
                DatOutputEntry entry;

                entry.countryCode = countryCode;
                entry.cell        = cell;
                entry.emissions.resize(_sectorIndexes.size(), 0.0);

                for (auto& [name, index] : _sectorIndexes) {
                    entry.emissions[index] += find_in_map_optional(sectorData, name).value_or(0.0);
                }

                entries.push_back(entry);
            }
        }

        const auto outputPath = _cfg.output_path() / create_chimere_output_name(_cfg.model_grid(), pol, _cfg.year(), _cfg.output_filename_suffix());
        write_dat_output(outputPath, entries);
    }

    _diffuseSources.clear();
}

std::vector<std::string> ChimereOutputBuilder::sector_names() const
{
    std::vector<std::string> sectorNames(_sectorIndexes.size());
    for (auto& [name, index] : _sectorIndexes) {
        sectorNames[index] = str::replace(name, " ", "");
    }

    return sectorNames;
}

void ChimereOutputBuilder::flush(WriteMode /*mode*/)
{
    write_dat_header(_cfg.output_path() / "output_Chimere_header.dat", sector_names());

    if (_pointSources.empty()) {
        return;
    }

    std::vector<std::string> pollutants(_pollutantIndexes.size());
    for (auto& [pol, index] : _pollutantIndexes) {
        pollutants[index] = pol.code();
    }

    const auto pointSourceOutputPath = _cfg.output_path() / create_chimere_point_source_output_name(_cfg.year(), _cfg.output_filename_suffix());
    write_dat_output(pointSourceOutputPath, _pointSources, pollutants);
    _pointSources.clear();
}

}
