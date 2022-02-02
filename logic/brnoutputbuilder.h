#pragma once

#include "brnoutputentry.h"
#include "emap/outputbuilderinterface.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace emap {

class BrnOutputBuilder : public IOutputBuilder
{
public:
    struct SectorParameterConfig
    {
        double hc_MW = 0.0;
        double h_m   = 0.0;
        double s_m   = 0.0;
        double tb    = 0.0;
    };

    struct PollutantParameterConfig
    {
        int32_t sd = 0;
    };

    BrnOutputBuilder(std::unordered_map<int32_t, SectorParameterConfig> sectorParams,
                     std::unordered_map<std::string, PollutantParameterConfig> pollutantParams,
                     int32_t cellSizeInM);

    void add_point_output_entry(const EmissionEntry& emission) override;
    void add_diffuse_output_entry(const EmissionIdentifier& id, int64_t x, int64_t y, double emission) override;

    void write_to_disk(const RunConfiguration& cfg) override;

private:
    std::mutex _mutex;
    int32_t _cellSizeInM = 0;

    std::unordered_map<Pollutant, std::vector<BrnOutputEntry>> _diffuseSources;
    std::unordered_map<Pollutant, std::vector<BrnOutputEntry>> _pointSources;
    std::unordered_map<int32_t, SectorParameterConfig> _sectorParams;
    std::unordered_map<std::string, PollutantParameterConfig> _pollutantParams;
};

}