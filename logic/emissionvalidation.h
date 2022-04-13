#pragma once

#include "emap/emissioninventory.h"
#include "emap/runconfiguration.h"
#include "gdx/denseraster.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace emap {

class EmissionValidation
{
public:
    struct SummaryEntry
    {
        double diff() const noexcept
        {
            return (inventory_total() - spreadDiffuseOutsideOfGridTotal.value_or(0.0)) - spread_total();
        }

        double diff_from_output() const noexcept
        {
            return (inventory_total() - spreadDiffuseOutsideOfGridTotal.value_or(0.0)) - outputTotal.value_or(0.0);
        }

        double inventory_total() const noexcept
        {
            return emissionInventoryDiffuse + emissionInventoryPoint;
        }

        double spread_total() const noexcept
        {
            return spreadDiffuseTotal.value_or(0.0) + spreadPointTotal.value_or(0.0);
        }

        EmissionIdentifier id;
        double emissionInventoryDiffuse = 0.0;
        double emissionInventoryPoint   = 0.0;
        std::optional<double> spreadDiffuseTotal;
        std::optional<double> spreadDiffuseOutsideOfGridTotal;
        std::optional<double> spreadPointTotal;

        std::optional<double> outputTotal;
    };

    EmissionValidation(const RunConfiguration& cfg);

    void add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal);
    void add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster, double insideGridRatio);
    void set_grid_countries(const std::unordered_set<CountryId>& countries);

    std::vector<SummaryEntry> create_summary(const EmissionInventory& emissionInv);

private:
    std::mutex _mutex;
    const RunConfiguration& _cfg;
    std::unordered_set<CountryId> _gridCountries;
    std::unordered_map<EmissionIdentifier, double> _diffuseEmissionSums;
    std::unordered_map<EmissionIdentifier, double> _diffuseEmissionOutsideGridSums;
    std::unordered_map<EmissionIdentifier, double> _pointEmissionSums;
};

}