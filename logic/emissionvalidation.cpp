#include "emissionvalidation.h"
#include "brnanalyzer.h"
#include "emap/configurationparser.h"
#include "emap/modelpaths.h"
#include "gdx/algo/sum.h"
#include "outputreaders.h"

#include "infra/exception.h"
#include "infra/log.h"

namespace emap {

using namespace inf;

EmissionValidation::EmissionValidation(const RunConfiguration& cfg)
: _cfg(cfg)
{
}

void EmissionValidation::add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal)
{
    std::scoped_lock lock(_mutex);
    _pointEmissionSums[id] += pointEmissionsTotal;
}

void EmissionValidation::add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster, double emissionsOutsideOfTheGrid)
{
    const auto sum = gdx::sum(raster);

    std::scoped_lock lock(_mutex);
    _diffuseEmissionSums[id] += sum;
    _diffuseEmissionOutsideGridSums[id] += emissionsOutsideOfTheGrid;
}

void EmissionValidation::set_grid_countries(const std::unordered_set<CountryId>& countries)
{
    _gridCountries = countries;
}

std::vector<EmissionValidation::SummaryEntry> EmissionValidation::create_summary(const EmissionInventory& emissionInv)
{
    std::vector<EmissionValidation::SummaryEntry> result;
    result.reserve(emissionInv.size());

    auto includedPollutants         = _cfg.included_pollutants();
    const auto sectorParametersPath = ModelPaths(_cfg.scenario(), _cfg.data_root(), _cfg.output_path()).sector_parameters_config_path();
    const auto sectorParams         = parse_sector_parameters_config(sectorParametersPath, _cfg.output_sector_level(), _cfg.pollutants(), _cfg.output_sector_level_name());

    std::unordered_map<Pollutant, std::unordered_map<CountrySector, double>> brnTotals;

    if (_cfg.model_output_format() == ModelOuputFormat::Brn && _cfg.output_sector_level() == SectorLevel::NFR) {
        for (auto& pol : includedPollutants) {
            const auto path = _cfg.output_path() / fmt::format("{}_OPS_{}{}.brn", pol.code(), static_cast<int>(_cfg.year()), _cfg.output_filename_suffix());
            try {
                if (fs::exists(path)) {
                    const auto brnEntries = read_brn_output(path);
                    BrnAnalyzer analyzer(brnEntries);
                    brnTotals.emplace(pol, analyzer.create_totals());
                } else {
                    brnTotals.emplace(pol, std::unordered_map<CountrySector, double>());
                }
            } catch (const std::exception& e) {
                throw RuntimeError("Error parsing brn {}: ({})", path, e.what());
            }
        }
    } else {
        Log::warn("Validation not implemented for this run configuration");
    }

    for (auto& invEntry : emissionInv) {
        auto& emissionId = invEntry.id();

        if (_gridCountries.count(emissionId.country.id()) == 0 || std::find(includedPollutants.begin(), includedPollutants.end(), emissionId.pollutant) == includedPollutants.end()) {
            continue;
        }

        EmissionValidation::SummaryEntry summaryEntry;
        summaryEntry.id                       = invEntry.id();
        summaryEntry.emissionInventoryDiffuse = invEntry.scaled_diffuse_emissions_sum();
        summaryEntry.emissionInventoryPoint   = invEntry.scaled_point_emissions_sum();

        summaryEntry.spreadDiffuseTotal              = find_in_map_optional(_diffuseEmissionSums, summaryEntry.id);
        summaryEntry.spreadDiffuseOutsideOfGridTotal = find_in_map_optional(_diffuseEmissionOutsideGridSums, summaryEntry.id);
        summaryEntry.spreadPointTotal                = find_in_map_optional(_pointEmissionSums, summaryEntry.id);

        if (_cfg.output_sector_level() == SectorLevel::NFR) {
            int32_t countryCode = static_cast<int32_t>(invEntry.id().country.id());
            int32_t sectorCode  = sectorParams.get_parameters(_cfg.sectors().map_nfr_to_output_name(invEntry.id().sector.nfr_sector()), invEntry.id().pollutant).id;

            summaryEntry.outputTotal = brnTotals[invEntry.id().pollutant][CountrySector(countryCode, sectorCode)];
        }

        result.push_back(summaryEntry);
    }

    return result;
}

}