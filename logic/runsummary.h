#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include "emissionvalidation.h"
#include "spatialpatterninventory.h"

#include <unordered_map>
#include <vector>

struct lxw_workbook;

namespace emap {

class RunSummary
{
public:
    void add_spatial_pattern_source(const SpatialPatternSource& source);
    void add_spatial_pattern_source_without_data(const SpatialPatternSource& source);
    void add_point_source(const fs::path& pointSource);
    void add_totals_source(const fs::path& totalsSource);

    void add_gnfr_correction(const EmissionIdentifier& id, std::optional<double> validatedGnfrTotal, double summedGnfrTotal, double correction);

    void set_validation_results(std::vector<EmissionValidation::SummaryEntry> results);

    void write_summary(const fs::path& outputDir) const;

private:
    struct GnfrCorrection
    {
        EmissionIdentifier id;
        std::optional<double> validatedGnfrTotal;
        double summedGnfrTotal = 0.0;
        double correction      = 0.0;
    };

    void gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const;
    void validation_results_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const EmissionValidation::SummaryEntry> validationResults) const;
    void emission_sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName) const;
    void write_summary_spreadsheet(const fs::path& path) const;

    std::vector<SpatialPatternSource> _spatialPatterns;
    std::vector<SpatialPatternSource> _spatialPatternsWithoutData;
    std::vector<fs::path> _pointSources;
    std::vector<fs::path> _totalsSources;
    std::vector<GnfrCorrection> _gnfrCorrections;
    std::vector<EmissionValidation::SummaryEntry> _validationResults;
};

}
