#include "emissionscollector.h"
#include "emap/outputbuilderfactory.h"

#include "gdx/denserasterio.h"
#include "gdx/rasterarea.h"
#include "infra/cast.h"

namespace emap {

using namespace inf;

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

void EmissionsCollector::add_point_emissions(std::span<const EmissionEntry> entries)
{
    for (auto& entry : entries) {
        _outputBuilder->add_point_output_entry(entry);
    }
}

void EmissionsCollector::add_diffuse_emissions(const Country& country, const NfrSector& nfr, gdx::DenseRaster<double> raster)
{
    assert(_pollutant.has_value());
    if (raster.contains_only_nodata()) {
        return;
    }

    const auto& meta = raster.metadata();

    EmissionIdentifier emissionId(country, EmissionSector(nfr), *_pollutant);

    for (auto cell : gdx::RasterCells(raster)) {
        if (raster.is_nodata(cell) || raster[cell] == 0.0) {
            continue;
        }

        const auto cellCenter = meta.convert_cell_centre_to_xy(cell);
        _outputBuilder->add_diffuse_output_entry(emissionId, Point(truncate<int64_t>(cellCenter.x), truncate<int64_t>(cellCenter.y)), raster[cell], truncate<int32_t>(meta.cell_size_x()));
    }

    if (_cfg.output_sector_level() == SectorLevel::NFR) {
        if (_cfg.output_country_rasters()) {
            gdx::write_raster(std::move(raster), _cfg.output_path_for_country_raster(emissionId, *_grid));
        }
    } else if (_cfg.output_grid_rasters()) {
        // The emissions need to be aggregated
        auto mappedSectorName = _cfg.sectors().map_nfr_to_output_name(nfr);

        std::scoped_lock lock(_mutex);
        if (auto iter = _collectedEmissions.find(mappedSectorName); iter != _collectedEmissions.end()) {
            add_to_raster(iter->second, raster);
        } else {
            gdx::DenseRaster<double> total(_grid->meta, std::numeric_limits<double>::quiet_NaN());
            add_to_raster(total, raster);
            _collectedEmissions.emplace(mappedSectorName, std::move(total));
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

    _collectedEmissions.clear();
    _pollutant.reset();
    _grid.reset();
}

void EmissionsCollector::final_flush_to_disk(WriteMode mode)
{
    _outputBuilder->flush(convert_write_mode(mode));
}

}