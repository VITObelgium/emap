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

RunSummary::RunSummary(const RunConfiguration& cfg)
: _cfg(&cfg)
{
}

void RunSummary::add_spatial_pattern_source(const SpatialPatternSource& source, double totalEmissions, double emissionsWithinGrid, double pointEmissions)
{
    SpatialPatternSummaryInfo info;
    info.source              = source;
    info.totalEmissions      = totalEmissions;
    info.emissionsWithinGrid = emissionsWithinGrid;
    info.pointEmissions      = pointEmissions;

    std::scoped_lock lock(_mutex);
    _spatialPatterns.push_back(info);
}

void RunSummary::add_spatial_pattern_source_without_data(const SpatialPatternSource& source, double totalEmissions, double emissionsWithinGrid, double pointEmissions)
{
    SpatialPatternSummaryInfo info;
    info.source              = source;
    info.totalEmissions      = totalEmissions;
    info.emissionsWithinGrid = emissionsWithinGrid;
    info.pointEmissions      = pointEmissions;

    std::scoped_lock lock(_mutex);
    _spatialPatternsWithoutData.push_back(info);
}

void RunSummary::add_point_source(const fs::path& pointSource)
{
    _pointSources.insert(pointSource);
}

void RunSummary::add_totals_source(const fs::path& totalsSource)
{
    _totalsSources.insert(totalsSource);
}

void RunSummary::add_gnfr_correction(const EmissionIdentifier& id, std::optional<double> validatedGnfrTotal, double summedGnfrTotal, double correction)
{
    _gnfrCorrections.push_back({id, validatedGnfrTotal, summedGnfrTotal, correction});
}

void RunSummary::add_gnfr_correction(const EmissionIdentifier& id, double validatedGnfrTotal, double correctedGnfrTotal, double nfrTotal, double olderNfrTotal)
{
    ValidatedGnfrCorrection correction;
    correction.id                 = id;
    correction.validatedGnfrTotal = validatedGnfrTotal;
    correction.correctedGnfrTotal = correctedGnfrTotal;
    correction.nfrTotal           = nfrTotal;
    correction.olderNfrTotal      = olderNfrTotal;
    _validatedGnfrCorrections.push_back(correction);
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
    case emap::SpatialPatternSource::Type::SpatialPatternFlanders:
        return "Flanders Excel";
    case emap::SpatialPatternSource::Type::UnfiformSpread:
        return "Uniform spread";
    case emap::SpatialPatternSource::Type::Raster:
        return "Raster";
    }

    return "";
}

void RunSummary::sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const SpatialPatternSummaryInfo> sources, std::span<const SpatialPatternSummaryInfo> sourcesWithoutData) const
{
    const std::array<ColumnInfo, 14> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Sector", 15.0},
        ColumnInfo{"GNFR", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"Used Sector", 15.0},
        ColumnInfo{"Used Pollutant", 15.0},
        ColumnInfo{"Type", 15.0},
        ColumnInfo{"Uniform spread fallback", 25.0},
        ColumnInfo{"From exceptions", 25.0},
        ColumnInfo{"Year", 15.0},
        ColumnInfo{"Path", 125.0},
        ColumnInfo{"Total emissions", 17.0},
        ColumnInfo{"Emissions within grid", 17.0},
        ColumnInfo{"Point Emissions", 17.0},
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

    auto addSource = [&](const SpatialPatternSummaryInfo& info, bool dataUsed) {
        std::string country(info.source.emissionId.country.iso_code());
        std::string sector(info.source.emissionId.sector.name());
        std::string gnfr(info.source.emissionId.sector.gnfr_name());
        std::string pollutant(info.source.emissionId.pollutant.code());
        std::string type = spatial_pattern_source_type_to_string(info.source.type);

        std::string usedSector(info.source.usedEmissionId.sector.name());
        std::string usedPollutant(info.source.usedEmissionId.pollutant.code());

        int index = 0;
        worksheet_write_string(ws, row, index++, country.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, sector.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, gnfr.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, usedSector.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, usedPollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, index++, type.c_str(), nullptr);
        worksheet_write_boolean(ws, row, index++, (!dataUsed) || info.source.patternAvailableButWithoutData, nullptr);
        worksheet_write_boolean(ws, row, index++, info.source.isException, nullptr);
        if (info.source.year.has_value()) {
            worksheet_write_number(ws, row, index++, static_cast<int>(*info.source.year), nullptr);
        } else {
            ++index;
        }
        worksheet_write_string(ws, row, index++, str::from_u8(info.source.path.generic_u8string()).c_str(), nullptr);

        worksheet_write_number(ws, row, index++, info.totalEmissions, formatNumber);
        worksheet_write_number(ws, row, index++, info.emissionsWithinGrid, formatNumber);
        worksheet_write_number(ws, row, index++, info.pointEmissions, formatNumber);
    };

    for (const auto& info : sources) {
        addSource(info, true);
        ++row;
    }

    for (const auto& info : sourcesWithoutData) {
        addSource(info, false);
        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}

void RunSummary::gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const
{
    if (corrections.empty()) {
        return;
    }

    const std::array<ColumnInfo, 7> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"NFR", 15.0},
        ColumnInfo{"GNFR", 15.0},
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
        worksheet_write_string(ws, row, 3, sector.c_str(), nullptr);
        if (correction.validatedGnfrTotal.has_value()) {
            worksheet_write_number(ws, row, 4, *correction.validatedGnfrTotal, formatNumber);
        }
        worksheet_write_number(ws, row, 5, correction.summedGnfrTotal, formatNumber);
        if (std::isfinite(correction.correction)) {
            worksheet_write_number(ws, row, 6, correction.correction, formatNumber);
        }

        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}

void RunSummary::validated_gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const ValidatedGnfrCorrection> corrections) const
{
    if (corrections.empty()) {
        return;
    }

    auto year       = static_cast<int>(_cfg->year());
    auto reportYear = static_cast<int>(_cfg->reporting_year());

    std::string validatedHeader = fmt::format("Validated GNFR_{}_{}", year - 1, reportYear - 1);
    std::string correctedHeader = fmt::format("Corrected GNFR_{}_{}", year, reportYear);

    std::string nfrSumHeader      = fmt::format("NFR_{}_{} sum", year, reportYear);
    std::string nfrSumOlderHeader = fmt::format("NFR_{}_{} sum", year - 1, reportYear);

    const std::array<ColumnInfo, 8> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"NFR", 15.0},
        ColumnInfo{"GNFR", 15.0},
        ColumnInfo{validatedHeader.c_str(), 30.0},
        ColumnInfo{nfrSumHeader.c_str(), 15.0},
        ColumnInfo{nfrSumOlderHeader.c_str(), 15.0},
        ColumnInfo{correctedHeader.c_str(), 30.0},
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
        std::string gnfr(correction.id.sector.gnfr_name());
        std::string pollutant(correction.id.pollutant.code());

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, sector.c_str(), nullptr);
        worksheet_write_string(ws, row, 3, gnfr.c_str(), nullptr);
        worksheet_write_number(ws, row, 4, correction.validatedGnfrTotal, formatNumber);
        worksheet_write_number(ws, row, 5, correction.nfrTotal, formatNumber);
        worksheet_write_number(ws, row, 6, correction.olderNfrTotal, formatNumber);
        worksheet_write_number(ws, row, 7, correction.correctedGnfrTotal, formatNumber);

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
    validated_gnfr_corrections_to_spreadsheet(wb, "GNFR emission correction", _validatedGnfrCorrections);
    gnfr_corrections_to_spreadsheet(wb, "NFR emission correction", _gnfrCorrections);
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

    const std::array<ColumnInfo, 7> headers = {
        ColumnInfo{"Country", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"NFR", 15.0},
        ColumnInfo{"GNFR", 15.0},
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
        std::string gnfr(emissionId.sector.gnfr_name());
        std::string pollutant(emissionId.pollutant.code());
        std::string country(emissionId.country.iso_code());

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, sector.c_str(), nullptr);
        worksheet_write_string(ws, row, 3, gnfr.c_str(), nullptr);
        worksheet_write_number(ws, row, 4, summaryEntry.emissionInventoryTotal, formatNumber);
        if (summaryEntry.spreadTotal.has_value()) {
            worksheet_write_number(ws, row, 5, *summaryEntry.spreadTotal, formatNumber);
            worksheet_write_number(ws, row, 6, std::abs(summaryEntry.diff()), formatNumber);
        }

        ++row;
    }

    worksheet_autofilter(ws, 0, 0, row, truncate<lxw_col_t>(headers.size() - 1));
}
}
