#pragma once

#include "emap/emissioninventory.h"
#include "emap/runconfiguration.h"
#include "gdx/denseraster.h"

#include <mutex>
#include <unordered_map>

namespace emap {

class EmissionValidation
{
public:
    struct SummaryEntry
    {
        double diff() const noexcept
        {
            return inventory_total() - spread_total();
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
        std::optional<double> spreadPointTotal;
    };

    void add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal);
    void add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster);

    std::vector<SummaryEntry> create_summary(const EmissionInventory& emissionInv);

private:
    std::mutex _mutex;
    std::unordered_map<EmissionIdentifier, double> _diffuseEmissionSums;
    std::unordered_map<EmissionIdentifier, double> _pointEmissionSums;
};

}