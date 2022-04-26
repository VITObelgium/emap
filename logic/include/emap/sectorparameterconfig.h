#pragma once

#include "emap/pollutant.h"

#include <cstdint>

namespace emap {

struct SectorParameters
{
    double hc_MW = 0.0;
    double h_m   = 0.0;
    double s_m   = 0.0;
    double tb    = 0.0;
    int32_t id   = 0;
};

class SectorParameterConfiguration
{
public:
    void add_parameter(const std::string& sector, const SectorParameters& params);
    void add_pollutant_specific_parameter(const std::string& sector, const Pollutant& pollutant, const SectorParameters& params);

    SectorParameters get_parameters(const std::string& sector, const Pollutant& pollutant) const;
    std::vector<std::string> sector_names_sorted_by_id() const;

private:
    static Pollutant AnyPollutant;

    struct PollutantSectorParameters : public SectorParameters
    {
        Pollutant pollutant;
    };

    std::unordered_map<std::string, std::vector<PollutantSectorParameters>> _parameters;
};

}