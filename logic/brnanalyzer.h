#pragma once

#include "brnoutputentry.h"
#include "emap/constants.h"
#include "infra/span.h"

#include <vector>

namespace emap {

class BrnAnalyzer
{
public:
    BrnAnalyzer(std::span<const BrnOutputEntry> entries)
    : _entries(entries)
    {
    }

    size_t size() const noexcept
    {
        return _entries.size();
    }

    double total_sum(int32_t countryId, int32_t sectorId) const noexcept
    {
        return diffuse_emissions_sum(countryId, sectorId) + point_emissions_sum(countryId, sectorId);
    }

    double diffuse_emissions_sum(int32_t countryId, int32_t sectorId) const noexcept
    {
        double sum = 0.0;

        for (auto& entry : _entries) {
            if (entry.d_m != 0 && entry.cat == sectorId && entry.area == countryId) {
                sum += entry.q_gs;
            }
        }

        return to_giga_gram(sum);
    }

    double point_emissions_sum(int32_t countryId, int32_t sectorId) const noexcept
    {
        double sum = 0.0;

        for (auto& entry : _entries) {
            if (entry.d_m == 0 && entry.cat == sectorId && entry.area == countryId) {
                sum += entry.q_gs;
            }
        }

        return to_giga_gram(sum);
    }

private:
    static double to_giga_gram(double val)
    {
        return val / constants::toGramPerYearRatio;
    }

    std::span<const BrnOutputEntry> _entries;
};

}