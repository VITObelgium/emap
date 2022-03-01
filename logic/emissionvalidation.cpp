#include "emissionvalidation.h"
#include "gdx/algo/sum.h"
#include "infra/exception.h"
#include "xlsxworkbook.h"

namespace emap {

using namespace inf;

void EmissionValidation::add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal)
{
    std::scoped_lock lock(_mutex);
    _emissionSums[id] += pointEmissionsTotal;
}

void EmissionValidation::add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster)
{
    auto sum = gdx::sum(raster);

    std::scoped_lock lock(_mutex);
    _emissionSums[id] += sum;
}

void EmissionValidation::write_summary(const EmissionInventory& emissionInv, const fs::path& outputPath) const
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
    fs::remove(outputPath, ec);

    xl::WorkBook wb(outputPath.generic_u8string());
    auto* ws = workbook_add_worksheet(wb, "Validation");
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
    for (auto& [emissionId, spreadEmission] : _emissionSums) {
        auto sourceEmission = emissionInv.emission_with_id(emissionId).scaled_diffuse_emissions_sum();

        std::string sector(emissionId.sector.name());
        std::string pollutant(emissionId.pollutant.code());
        std::string country(emissionId.country.iso_code());

        worksheet_write_string(ws, row, 0, country.c_str(), nullptr);
        worksheet_write_string(ws, row, 1, pollutant.c_str(), nullptr);
        worksheet_write_string(ws, row, 2, sector.c_str(), nullptr);
        worksheet_write_number(ws, row, 3, sourceEmission, nullptr);
        worksheet_write_number(ws, row, 4, spreadEmission, nullptr);
        worksheet_write_number(ws, row, 5, std::abs(spreadEmission - sourceEmission), nullptr);

        ++row;
    }
}

}