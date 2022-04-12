#pragma once

#include "emap/runconfiguration.h"
#include "gdx/denseraster.h"

#include <memory>
#include <optional>

namespace emap {

class IOutputBuilder;
struct CountryCellCoverage;

class EmissionsCollector
{
public:
    enum class WriteMode
    {
        Create,
        Append,
    };

    EmissionsCollector(const RunConfiguration& cfg);
    ~EmissionsCollector() noexcept;

    void start_pollutant(const Pollutant& pol, const GridData& grid);

    void add_emissions(const CountryCellCoverage& countryInfo, const NfrSector& nfr, gdx::DenseRaster<double> diffuseEmissions, std::span<const EmissionEntry> pointEmissions);

    void flush_pollutant_to_disk(WriteMode mode);
    void final_flush_to_disk(WriteMode mode);

private:
    std::mutex _mutex;

    const RunConfiguration& _cfg;
    std::optional<Pollutant> _pollutant;
    std::optional<GridData> _grid;
    std::unordered_map<std::string, gdx::DenseRaster<double>> _collectedEmissions;
    std::map<std::pair<std::string, std::string>, gdx::DenseRaster<double>> _collectedCountryEmissions;
    std::unique_ptr<IOutputBuilder> _outputBuilder;
};

}