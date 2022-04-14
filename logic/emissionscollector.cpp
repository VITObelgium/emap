#include "emissionscollector.h"
#include "emap/gridprocessing.h"
#include "emap/outputbuilderfactory.h"

#include "gdx/denserasterio.h"
#include "gdx/rasterarea.h"
#include "infra/cast.h"
#include "infra/log.h"

namespace emap {

using namespace inf;

static void add_point_sources_to_grid(const EmissionIdentifier& id, std::span<const EmissionEntry> pointEmissions, gdx::DenseRaster<double>& raster)
{
    const auto& meta = raster.metadata();

    size_t mismatches = 0;

    // Add the point sources to the grid
    for (auto pointEmission : pointEmissions) {
        if (auto amount = pointEmission.value().amount(); amount.has_value()) {
            if (auto coord = pointEmission.coordinate(); coord.has_value()) {
                auto cell = meta.convert_xy_to_cell(coord->x, coord->y);
                if (meta.is_on_map(cell)) {
                    if (raster.is_nodata(cell)) {
                        raster[cell] = *amount;
                        raster.mark_as_data(cell);
                    } else {
                        raster[cell] += *amount;
                    }
                } else {
                    Log::debug("Point source not on map: {} (Cell {} Grid rows {} cols {})", *coord, cell, meta.rows, meta.cols);
                    ++mismatches;
                }
            }
        }
    }

    if (mismatches > 0) {
        Log::warn("{}: Not all point sources could be added to the map: {} point sources, skipped {}", id, pointEmissions.size(), mismatches);
    }
}

EmissionsCollector::EmissionsCollector(const RunConfiguration& cfg)
: _cfg(cfg)
, _outputBuilder(make_output_builder(cfg))
{
}

EmissionsCollector::~EmissionsCollector() noexcept = default;

void EmissionsCollector::start_pollutant(const Pollutant& pol, const GridData& grid)
{
    _pollutant = pol;
    _grid      = grid;
}

static void add_to_raster(gdx::DenseRaster<double>& collectedRaster, const gdx::DenseRaster<double>& countryRaster)
{
    auto intersection = inf::metadata_intersection(collectedRaster.metadata(), countryRaster.metadata());
    if (intersection.rows == 0 || intersection.cols == 0) {
        return;
    }

    auto subGrid1 = gdx::sub_area(collectedRaster, intersection);
    auto subGrid2 = gdx::sub_area(countryRaster, intersection);
    std::transform(subGrid1.begin(), subGrid1.end(), subGrid2.begin(), subGrid1.begin(), [](double res, double toAdd) {
        if (std::isnan(toAdd)) {
            return res;
        }

        if (std::isnan(res)) {
            return toAdd;
        }

        return res + toAdd;
    });
}

void EmissionsCollector::add_emissions(const CountryCellCoverage& countryInfo, const NfrSector& nfr, gdx::DenseRaster<double> diffuseEmissions, std::span<const EmissionEntry> pointEmissions)
{
    assert(_pollutant.has_value());
    if (diffuseEmissions.contains_only_nodata()) {
        return;
    }

    const auto& meta = diffuseEmissions.metadata();

    EmissionIdentifier emissionId(countryInfo.country, EmissionSector(nfr), *_pollutant);

    for (auto cell : gdx::RasterCells(diffuseEmissions)) {
        if (diffuseEmissions.is_nodata(cell) || diffuseEmissions[cell] == 0.0) {
            continue;
        }

        const auto cellCenter = meta.convert_cell_centre_to_xy(cell);
        _outputBuilder->add_diffuse_output_entry(emissionId, Point(cellCenter.x, cellCenter.y), diffuseEmissions[cell], truncate<int32_t>(meta.cell_size_x()));
    }

    for (auto& entry : pointEmissions) {
        _outputBuilder->add_point_output_entry(entry);
    }

    if (diffuseEmissions.empty() && !pointEmissions.empty()) {
        diffuseEmissions = gdx::DenseRaster<double>(countryInfo.outputSubgridExtent, std::numeric_limits<double>::quiet_NaN());
    }

    add_point_sources_to_grid(emissionId, pointEmissions, diffuseEmissions);

    if (!diffuseEmissions.empty() && _cfg.output_grid_rasters()) {
        // The emissions need to be aggregated
        auto mappedSectorName = _cfg.sectors().map_nfr_to_output_name(nfr);

        std::scoped_lock lock(_mutex);
        if (auto iter = _collectedEmissions.find(mappedSectorName); iter != _collectedEmissions.end()) {
            add_to_raster(iter->second, diffuseEmissions);
        } else {
            gdx::DenseRaster<double> total(_grid->meta, std::numeric_limits<double>::quiet_NaN());
            add_to_raster(total, diffuseEmissions);
            _collectedEmissions.emplace(mappedSectorName, std::move(total));
        }
    }

    if (!diffuseEmissions.empty() && _cfg.output_country_rasters()) {
        if (_cfg.output_sector_level() == SectorLevel::NFR) {
            // Sectors can be dumped without aggregation
            gdx::write_raster(std::move(diffuseEmissions), _cfg.output_path_for_country_raster(emissionId, *_grid));
        } else {
            // Aggregate the country data per mapped sector
            // The emissions need to be aggregated
            auto id = std::make_pair(std::string(emissionId.country.iso_code()), _cfg.sectors().map_nfr_to_output_name(nfr));

            std::scoped_lock lock(_mutex);
            if (auto iter = _collectedCountryEmissions.find(id); iter != _collectedCountryEmissions.end()) {
                add_to_raster(iter->second, diffuseEmissions);
            } else {
                _collectedCountryEmissions.emplace(id, std::move(diffuseEmissions));
            }
        }
    }
}

static IOutputBuilder::WriteMode convert_write_mode(EmissionsCollector::WriteMode mode) noexcept
{
    return mode == EmissionsCollector::WriteMode::Create ? IOutputBuilder::WriteMode::Create : IOutputBuilder::WriteMode::Append;
}

void EmissionsCollector::flush_pollutant_to_disk(WriteMode mode)
{
    assert(_pollutant.has_value());
    std::scoped_lock lock(_mutex);

    _outputBuilder->flush_pollutant(*_pollutant, convert_write_mode(mode));

    for (auto& [name, raster] : _collectedEmissions) {
        const auto outputPath = _cfg.output_dir_for_rasters() / fs::u8path(fmt::format("{}_{}_{}.tif", _pollutant->code(), name, _grid->name));
        gdx::write_raster(std::move(raster), outputPath);
    }

    for (auto& [id, raster] : _collectedCountryEmissions) {
        const auto outputPath = _cfg.output_dir_for_rasters() / fs::u8path(fmt::format("{}_{}_{}_{}.tif", _pollutant->code(), id.second, id.first, _grid->name));
        gdx::write_raster(std::move(raster), outputPath);
    }

    _collectedEmissions.clear();
    _collectedCountryEmissions.clear();
    _pollutant.reset();
    _grid.reset();
}

void EmissionsCollector::final_flush_to_disk(WriteMode mode)
{
    _outputBuilder->flush(convert_write_mode(mode));
}
}