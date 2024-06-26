#pragma once

#include "emap/emissions.h"
#include "infra/filesystem.h"

#include "emissionvalidation.h"
#include "spatialpatterninventory.h"

#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

struct lxw_workbook;

namespace emap {

class EmissionInventoryEntry;

class RunSummary
{
public:
    RunSummary() = default;
    RunSummary(const RunConfiguration& cfg);

    void add_spatial_pattern_source(const SpatialPatternSource& source, double scaledDiffuseEmissions, double scaledDiffuseEmissionsWithinGrid, const EmissionInventoryEntry& emission);
    void add_spatial_pattern_source_without_data(const SpatialPatternSource& source, double scaledDiffuseEmissions, double scaledDiffuseEmissionsWithinGrid, const EmissionInventoryEntry& emission);
    void add_point_source(const fs::path& pointSource);
    void add_totals_source(const fs::path& totalsSource);

    void add_gnfr_correction(const EmissionIdentifier& id, std::optional<double> validatedGnfrTotal, double summedGnfrTotal, double correction);
    void add_gnfr_correction(const EmissionIdentifier& id, double validatedGnfrTotal, double correctedGnfrTotal, double nfrTotal, double olderNfrTotal);

    void set_validation_results(std::vector<EmissionValidation::SummaryEntry> results);

    void write_summary(const fs::path& outputDir) const;

    const std::set<fs::path>& used_point_sources() const noexcept;

private:
    struct GnfrCorrection
    {
        EmissionIdentifier id;
        std::optional<double> validatedGnfrTotal;
        double summedGnfrTotal = 0.0;
        double correction      = 0.0;
    };

    struct ValidatedGnfrCorrection
    {
        EmissionIdentifier id;
        double validatedGnfrTotal = 0.0;
        double correctedGnfrTotal = 0.0;
        double nfrTotal           = 0.0;
        double olderNfrTotal      = 0.0;
    };

    struct SpatialPatternSummaryInfo
    {
        SpatialPatternSource source;
        double scaledDiffuseEmissions           = 0.0;
        double scaledDiffuseEmissionsWithinGrid = 0.0;
        double scaledPointEmissions             = 0.0;
        double diffuseScalingUser               = 1.0;
        double diffuseScalingAuto               = 1.0;
        double pointScalingUser                 = 1.0;
        double pointScalingAuto                 = 1.0;
    };

    void gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const;
    void validated_gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const ValidatedGnfrCorrection> corrections) const;
    void validation_results_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const EmissionValidation::SummaryEntry> validationResults) const;
    void emission_sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName) const;
    void sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const SpatialPatternSummaryInfo> sources, std::span<const SpatialPatternSummaryInfo> sourcesWithoutData) const;
    void write_summary_spreadsheet(const fs::path& path) const;

    const RunConfiguration* _cfg = nullptr;
    std::mutex _mutex;
    std::vector<SpatialPatternSummaryInfo> _spatialPatterns;
    std::vector<SpatialPatternSummaryInfo> _spatialPatternsWithoutData;
    std::set<fs::path> _pointSources;
    std::set<fs::path> _totalsSources;
    std::vector<GnfrCorrection> _gnfrCorrections;
    std::vector<ValidatedGnfrCorrection> _validatedGnfrCorrections;
    std::vector<EmissionValidation::SummaryEntry> _validationResults;
};

}
