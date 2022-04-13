#pragma once

#include "brnoutputentry.h"
#include "emap/outputbuilderinterface.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace emap {

class VlopsOutputBuilder : public IOutputBuilder
{
public:
    struct PollutantParameterConfig
    {
        int32_t sd = 0;
    };

    VlopsOutputBuilder(std::unordered_map<std::string, SectorParameterConfig> sectorParams,
                       std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                       const RunConfiguration& cfg);

    void add_point_output_entry(const EmissionEntry& emission) override;
    void add_diffuse_output_entry(const EmissionIdentifier& id, inf::Point<int64_t> loc, double emission, int32_t cellSizeInM) override;

    void flush_pollutant(const Pollutant& pol, WriteMode mode) override;
    void flush(WriteMode mode) override;

private:
    std::mutex _mutex;
    SectorLevel _sectorLevel;
    const RunConfiguration& _cfg;

    struct Entry
    {
        double value     = 0.0;
        int32_t cellSize = 0;
    };

    std::unordered_map<Pollutant, std::unordered_map<std::string, std::unordered_map<CountryId, std::unordered_map<inf::Point<int64_t>, Entry>>>> _diffuseSources;

    std::unordered_map<Pollutant, std::vector<BrnOutputEntry>> _pointSources;
    std::unordered_map<std::string, SectorParameterConfig> _sectorParams;
    std::unordered_map<std::string, PollutantParameterConfig> _pollutantParams;
};

}