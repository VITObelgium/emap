#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include "spatialpatterninventory.h"

#include <unordered_map>
#include <vector>

struct lxw_workbook;

namespace emap {

class RunSummary
{
public:
    void add_spatial_pattern_source(const SpatialPatternSource& source);
    void add_country_specific_spatial_pattern_source(const Country& country, const SpatialPatternSource& source);
    void add_point_source(const fs::path& pointSource);
    void add_totals_source(const fs::path& totalsSource);

    void add_gnfr_correction(const EmissionIdentifier& id, std::optional<double> validatedGnfrTotal, double summedGnfrTotal, double correction);

    std::string spatial_pattern_usage_table() const;
    std::string emission_source_usage_table() const;
    void write_summary_spreadsheet(const fs::path& path) const;

private:
    struct GnfrCorrection
    {
        EmissionIdentifier id;
        std::optional<double> validatedGnfrTotal;
        double summedGnfrTotal = 0.0;
        double correction      = 0.0;
    };

    void gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const;

    std::vector<SpatialPatternSource> _spatialPatterns;
    std::unordered_map<Country, std::vector<SpatialPatternSource>> _countrySpecificSpatialPatterns;
    std::vector<fs::path> _pointSources;
    std::vector<fs::path> _totalsSources;
    std::vector<GnfrCorrection> _gnfrCorrections;
};

}
