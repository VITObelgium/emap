#pragma once

#include "emap/emissions.h"
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
            return emissionInventoryTotal - spreadTotal.value_or(0.0);
        }

        EmissionIdentifier id;
        double gnfrTotal              = 0.0;
        double emissionInventoryTotal = 0.0;
        std::optional<double> spreadTotal;
    };

    void add_point_emissions(const EmissionIdentifier& id, double pointEmissionsTotal);
    void add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster);

    std::vector<SummaryEntry> create_summary(const EmissionInventory& emissionInv);

private:
    std::mutex _mutex;
    std::unordered_map<EmissionIdentifier, double> _emissionSums;
};

}