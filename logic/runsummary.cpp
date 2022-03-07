#include "runsummary.h"

#include "enuminfo.h"
#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/exception.h"
#include "infra/log.h"
#include "xlsxworkbook.h"

#include <algorithm>
#include <array>

namespace emap {

using namespace inf;

struct ColumnInfo
{
    const char* header = nullptr;
    double width       = 0.0;
};

void RunSummary::add_spatial_pattern_source(const SpatialPatternSource& source)
{
    _spatialPatterns.push_back(source);
}

void RunSummary::add_spatial_pattern_source_without_data(const SpatialPatternSource& source)
{
    _spatialPatternsWithoutData.push_back(source);
}

void RunSummary::add_point_source(const fs::path& pointSource)
{
    _pointSources.push_back(pointSource);
}

void RunSummary::add_totals_source(const fs::path& totalsSource)
{
    _totalsSources.push_back(totalsSource);
}

void RunSummary::add_gnfr_correction(const EmissionIdentifier& id, std::optional<double> validatedGnfrTotal, double summedGnfrTotal, double correction)
{
    _gnfrCorrections.push_back({id, validatedGnfrTotal, summedGnfrTotal, correction});
}

void RunSummary::set_validation_results(std::vector<EmissionValidation::SummaryEntry> results)
{
    _validationResults = std::move(results);
}

static std::string spatial_pattern_source_type_to_string(SpatialPatternSource::Type type)
{
    switch (type) {
    case emap::SpatialPatternSource::Type::SpatialPatternCAMS:
        return "CAMS";
    case emap::SpatialPatternSource::Type::SpatialPatternCEIP:
        return "CEIP";
    case emap::SpatialPatternSource::Type::SpatialPatternTable:
        return "Excel";
    case emap::SpatialPatternSource::Type::UnfiformSpread:
        return "Uniform spread";
    case emap::SpatialPatternSource::Type::RasterException:
        return "Exception";
    }

    return "";
}

static void sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const SpatialPatternSource> sources, std::span<const SpatialPatternSource> sourcesWithoutData)
{
    const std::array<ColumnInfo, 7> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Sector", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"Type", 15.0},
        ColumnInfo{"Uniform spread fallback", 25.0},
        ColumnInfo{"Year", 15.0},
        ColumnInfo{"Path", 125.0},
    };

    auto* ws = workbook_add_worksheet(wb, tabName.c_str());
    if (!ws) {
        throw RuntimeError("Failed to add sheet to excel document");
    }

    auto* headerFormat = workbook_add_format(wb);
    format_set_bold(headerFormat);
    format_set_bg_color(headerFormat, 0xD5EBFF);

    for (int i = 0; i < truncate<int>(headers.size()); ++i) {
        worksheet_set_column(ws, i, i, headers.at(i).width, nullptr);
        worksheet_write_string(ws, 0, i, headers.at(i).header, headerFormat);
    }

    int row = 1;

    auto addSource = [&](const SpatialPatternSource& sp, bool dataUsed) {
        std::string country(sp.emissionId.country.iso_code());
        std::string sector(sp.emissionId.sector.name());
        std::string pollutant(sp.emissionId.pollutant.code());
        std::string type = spatial_pattern_source_type_to_string(sp.type);

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, sector.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 3, type.c_str(), nullptr);
        worksheet_write_boolean(ws, row, 4, !dataUsed, nullptr);
        if (sp.type != SpatialPatternSource::Type::UnfiformSpread &&
            sp.type != SpatialPatternSource::Type::RasterException) {
            worksheet_write_number(ws, row, 5, static_cast<int>(sp.year), nullptr);
        }
        worksheet_write_string(ws, row, 6, str::from_u8(sp.path.generic_u8string()).c_str(), nullptr);
    };

    for (const auto& sp : sources) {
        addSource(sp, true);
        ++row;
    }

    for (const auto& sp : sourcesWithoutData) {
        addSource(sp, false);
        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}

void RunSummary::gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const
{
    const std::array<ColumnInfo, 6> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"Sector", 15.0},
        ColumnInfo{"Validated GNFR", 15.0},
        ColumnInfo{"NFR Sum", 15.0},
        ColumnInfo{"Scaling factor", 15.0},
    };

    auto* ws = workbook_add_worksheet(wb, tabName.c_str());
    if (!ws) {
        throw RuntimeError("Failed to add sheet to excel document");
    }

    auto* headerFormat = workbook_add_format(wb);
    format_set_bold(headerFormat);
    format_set_bg_color(headerFormat, 0xD5EBFF);

    auto* formatNumber = workbook_add_format(wb);
    format_set_num_format(formatNumber, "0.000000000");

    for (int i = 0; i < truncate<int>(headers.size()); ++i) {
        worksheet_set_column(ws, i, i, headers.at(i).width, nullptr);
        worksheet_write_string(ws, 0, i, headers.at(i).header, headerFormat);
    }

    int row = 1;
    for (const auto& correction : corrections) {
        std::string country(correction.id.country.iso_code());
        std::string sector(correction.id.sector.gnfr_sector().name());
        std::string pollutant(correction.id.pollutant.code());

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, sector.c_str(), nullptr);
        if (correction.validatedGnfrTotal.has_value()) {
            worksheet_write_number(ws, row, 3, *correction.validatedGnfrTotal, formatNumber);
        }
        worksheet_write_number(ws, row, 4, correction.summedGnfrTotal, formatNumber);
        if (std::isfinite(correction.correction)) {
            worksheet_write_number(ws, row, 5, correction.correction, formatNumber);
        }

        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}

void RunSummary::emission_sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName) const
{
    const std::array<ColumnInfo, 2> headers = {
        ColumnInfo{"Emission type", 15.0},
        ColumnInfo{"Path", 100.0},
    };

    auto* ws = workbook_add_worksheet(wb, tabName.c_str());
    if (!ws) {
        throw RuntimeError("Failed to add sheet to excel document");
    }

    auto* headerFormat = workbook_add_format(wb);
    format_set_bold(headerFormat);
    format_set_bg_color(headerFormat, 0xD5EBFF);

    for (int i = 0; i < truncate<int>(headers.size()); ++i) {
        worksheet_set_column(ws, i, i, headers.at(i).width, nullptr);
        worksheet_write_string(ws, 0, i, headers.at(i).header, headerFormat);
    }

    int row = 1;
    for (const auto& ps : _pointSources) {
        worksheet_write_string(ws, row, 0, "Point source", nullptr);
        worksheet_write_string(ws, row, 1, str::from_u8(ps.u8string()).c_str(), nullptr);
        ++row;
    }

    for (const auto& ps : _totalsSources) {
        worksheet_write_string(ws, row, 0, "Totals", nullptr);
        worksheet_write_string(ws, row, 1, str::from_u8(ps.u8string()).c_str(), nullptr);
        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}

void RunSummary::write_summary(const fs::path& outputDir) const
{
    write_summary_spreadsheet(outputDir / "summary.xlsx");
}

void RunSummary::write_summary_spreadsheet(const fs::path& path) const
{
    std::error_code ec;
    fs::remove(path, ec);

    xl::WorkBook wb(path);

    emission_sources_to_spreadsheet(wb, "emission sources");
    gnfr_corrections_to_spreadsheet(wb, "emission correction", _gnfrCorrections);
    sources_to_spreadsheet(wb, "spatial patterns", _spatialPatterns, _spatialPatternsWithoutData);
    validation_results_to_spreadsheet(wb, "result validation", _validationResults);
}

void RunSummary::validation_results_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const EmissionValidation::SummaryEntry> validationResults) const
{
    if (validationResults.empty()) {
        return;
    }

    struct ColumnInfo
    {
        const char* header = nullptr;
        double width       = 0.0;
    };

    const std::array<ColumnInfo, 6> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"Sector", 15.0},
        ColumnInfo{"Input emission", 15.0},
        ColumnInfo{"Output emission", 15.0},
        ColumnInfo{"Diff", 15.0},
    };

    auto* ws = workbook_add_worksheet(wb, tabName.c_str());
    if (!ws) {
        throw RuntimeError("Failed to add sheet to excel document");
    }

    auto* headerFormat = workbook_add_format(wb);
    format_set_bold(headerFormat);
    format_set_bg_color(headerFormat, 0xD5EBFF);

    auto* formatNumber = workbook_add_format(wb);
    format_set_num_format(formatNumber, "0.000000000");

    for (int i = 0; i < truncate<int>(headers.size()); ++i) {
        worksheet_set_column(ws, i, i, headers.at(i).width, nullptr);
        worksheet_write_string(ws, 0, i, headers.at(i).header, headerFormat);
    }

    int row = 1;
    for (const auto& summaryEntry : _validationResults) {
        const auto& emissionId = summaryEntry.id;

        std::string sector(emissionId.sector.name());
        std::string pollutant(emissionId.pollutant.code());
        std::string country(emissionId.country.iso_code());

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, sector.c_str(), nullptr);
        worksheet_write_number(ws, row, 3, summaryEntry.emissionInventoryTotal, formatNumber);
        if (summaryEntry.spreadTotal.has_value()) {
            worksheet_write_number(ws, row, 4, *summaryEntry.spreadTotal, formatNumber);
            worksheet_write_number(ws, row, 5, std::abs(summaryEntry.diff()), formatNumber);
        }

        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}
}
