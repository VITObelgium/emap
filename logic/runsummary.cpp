#include "runsummary.h"

#include "enuminfo.h"
#include "infra/cast.h"
#include "infra/enumutils.h"
#include "infra/exception.h"
#include "infra/log.h"
#include "xlsxworkbook.h"

#include <algorithm>
#include <array>
#include <fmt/printf.h>
#include <tabulate/table.hpp>

namespace emap {

using namespace inf;

void RunSummary::add_spatial_pattern_source(const SpatialPatternSource& source)
{
    _spatialPatterns.push_back(source);
}

void RunSummary::add_country_specific_spatial_pattern_source(const Country& country, const SpatialPatternSource& source)
{
    _countrySpecificSpatialPatterns[country].push_back(source);
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
    }

    return "";
}

static std::string sources_to_table(std::span<const SpatialPatternSource> sources)
{
    using namespace tabulate;

    Table table;
    table.add_row({"Sector", "Pollutant", "Type", "Year", "Path"});

    for (const auto& sp : sources) {
        std::string sector(sp.emissionId.sector.name());
        std::string pollutant(sp.emissionId.pollutant.code());
        std::string type = spatial_pattern_source_type_to_string(sp.type);
        std::string year = sp.type == SpatialPatternSource::Type::UnfiformSpread ? "-" : std::to_string(static_cast<int>(sp.year));

        table.add_row({sector, pollutant, type, year, str::from_u8(sp.path.filename().generic_u8string())});
    }

    std::stringstream ss;
    ss << table;
    return ss.str();
}

static void sources_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const SpatialPatternSource> sources)
{
    struct ColumnInfo
    {
        const char* header = nullptr;
        double width       = 0.0;
    };

    const std::array<ColumnInfo, 5> headers = {
        ColumnInfo{"Sector", 15.0},
        ColumnInfo{"Pollutant", 15.0},
        ColumnInfo{"Type", 15.0},
        ColumnInfo{"Year", 15.0},
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
    for (const auto& sp : sources) {
        std::string sector(sp.emissionId.sector.name());
        std::string pollutant(sp.emissionId.pollutant.code());
        std::string type = spatial_pattern_source_type_to_string(sp.type);

        worksheet_write_string(ws, row, 0, sector.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, type.c_str(), nullptr);
        if (sp.type != SpatialPatternSource::Type::UnfiformSpread) {
            worksheet_write_number(ws, row, 3, static_cast<int>(sp.year), nullptr);
        }
        worksheet_write_string(ws, row, 4, str::from_u8(sp.path.filename().generic_u8string()).c_str(), nullptr);

        ++row;
    }
}

void RunSummary::gnfr_corrections_to_spreadsheet(lxw_workbook* wb, const std::string& tabName, std::span<const GnfrCorrection> corrections) const
{
    struct ColumnInfo
    {
        const char* header = nullptr;
        double width       = 0.0;
    };

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
}

std::string RunSummary::spatial_pattern_usage_table() const
{
    using namespace tabulate;

    std::stringstream result;

    for (auto& [region, sources] : _countrySpecificSpatialPatterns) {
        result << fmt::format("{} ({})\n", region.full_name(), region.iso_code());
        result << sources_to_table(sources);
    }

    result << fmt::format("\nOther regions\n");
    result << sources_to_table(_spatialPatterns);

    return result.str();
}

std::string RunSummary::emission_source_usage_table() const
{
    using namespace tabulate;

    Table table;
    table.add_row({"Emission type", "Path"});

    for (const auto& ps : _pointSources) {
        table.add_row({"Point source", str::from_u8(ps.u8string())});
    }

    for (const auto& ps : _totalsSources) {
        table.add_row({"Totals", str::from_u8(ps.u8string())});
    }

    std::stringstream result;
    result << table;
    return result.str();
}

void RunSummary::write_summary(const fs::path& outputDir) const
{
    write_summary_spreadsheet(outputDir / "summary.xlsx");
    write_summary_text_file(outputDir / "summary.txt");

    if (!_validationResults.empty()) {
        write_validation_results(outputDir / "emission_validation.xlsx");
    }
}

void RunSummary::write_summary_spreadsheet(const fs::path& path) const
{
    std::error_code ec;
    fs::remove(path, ec);

    xl::WorkBook wb(path);

    for (auto& [region, sources] : _countrySpecificSpatialPatterns) {
        sources_to_spreadsheet(wb, std::string(region.iso_code()), sources);
    }

    sources_to_spreadsheet(wb, "rest", _spatialPatterns);
    gnfr_corrections_to_spreadsheet(wb, "emission correction", _gnfrCorrections);
}

void RunSummary::write_summary_text_file(const fs::path& path) const
{
    file::create_directory_for_file(path);

    file::Handle fp(path, "wt");
    if (fp.is_open()) {
        fmt::fprintf(fp, "Used emissions\n");
        fmt::fprintf(fp, emission_source_usage_table());
        fmt::fprintf(fp, "\nUsed spatial patterns\n");
        fmt::fprintf(fp, spatial_pattern_usage_table());
    }
}

void RunSummary::write_validation_results(const fs::path& path) const
{
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

    std::error_code ec;
    fs::remove(path, ec);

    xl::WorkBook wb(path);
    auto* ws = workbook_add_worksheet(wb, "Validation");
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
            worksheet_write_number(ws, row, 5, summaryEntry.diff(), formatNumber);
        }

        ++row;
    }
}

}
