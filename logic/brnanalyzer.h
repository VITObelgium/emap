#pragma once

#include "brnoutputentry.h"
#include "emap/constants.h"
#include "infra/span.h"

#include <unordered_map>
#include <vector>

namespace emap {

struct CountrySector
{
    CountrySector() noexcept = default;
    CountrySector(int32_t c, int32_t s) noexcept
    : country(c)
    , sector(s)
    {
    }

    bool operator==(const CountrySector& other) const noexcept
    {
        return country == other.country && sector == other.sector;
    }

    int32_t country = 0;
    int32_t sector  = 0;
};
}

namespace std {
template <>
struct hash<emap::CountrySector>
{
    size_t operator()(const emap::CountrySector& val) const
    {
        size_t country = size_t(val.country) << 32;
        size_t sector  = val.sector;

        return hash<long long>()(country ^ sector);
    }
};
}

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

    std::unordered_map<CountrySector, double> create_totals()
    {
        std::unordered_map<CountrySector, double> totals;

        for (auto& entry : _entries) {
            totals[CountrySector(entry.area, entry.cat)] += entry.q_gs;
        }

        for (auto& [id, total] : totals) {
            total = to_giga_gram(total);
        }

        return totals;
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