#pragma once

#include "emap/runconfiguration.h"
#include "gdx/denseraster.h"

namespace emap {

class IOutputBuilder;

class EmissionsCollector
{
public:
    enum class WriteMode
    {
        Create,
        Append,
    };

    EmissionsCollector(const RunConfiguration& cfg, const Pollutant& pol, const GridData& grid);
    ~EmissionsCollector() noexcept;

    void add_diffuse_emissions(const Country& country, const NfrSector& nfr, gdx::DenseRaster<double> raster);
    void write_to_disk(WriteMode mode);

private:
    std::mutex _mutex;
    GridData _grid;

    const RunConfiguration& _cfg;
    Pollutant _pollutant;
    std::unordered_map<std::string, gdx::DenseRaster<double>> _collectedEmissions;
    std::unique_ptr<IOutputBuilder> _outputBuilder;
};

}