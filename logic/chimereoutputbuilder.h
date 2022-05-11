#pragma once

#include "datoutputentry.h"
#include "emap/outputbuilderinterface.h"
#include "infra/cell.h"

#include <cstdint>
#include <mutex>
#include <string>

#include <unordered_map>

namespace emap {

class ChimereOutputBuilder : public IOutputBuilder
{
public:
    ChimereOutputBuilder(SectorParameterConfiguration sectorParams,
                         std::unordered_map<CountryId, int32_t> countryMapping,
                         const RunConfiguration& cfg);

    void add_point_output_entry(const EmissionEntry& emission) override;
    void add_diffuse_output_entry(const EmissionIdentifier& id, inf::Point<double> loc, double emission, int32_t cellSizeInM) override;

    void flush_pollutant(const Pollutant& pol, WriteMode mode) override;
    void flush(WriteMode mode) override;

private:
    std::mutex _mutex;
    SectorLevel _sectorLevel;
    const RunConfiguration& _cfg;
    inf::GeoMetadata _meta;

    // [pollutant][country][cell][sector] -> value
    std::unordered_map<Pollutant, std::unordered_map<int32_t, std::unordered_map<inf::Cell, std::unordered_map<std::string, double>>>> _diffuseSources;

    std::vector<DatPointSourceOutputEntry> _pointSources;
    std::unordered_map<CountryId, int32_t> _countryMapping;
    SectorParameterConfiguration _sectorParams;
    std::unordered_map<std::string, size_t> _sectorIndexes;
    std::unordered_map<Pollutant, size_t> _pollutantIndexes;
};

}