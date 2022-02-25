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
    void add_diffuse_emissions(const EmissionIdentifier& id, const gdx::DenseRaster<double>& raster);
    void write_summary(const EmissionInventory& emissionInv, const fs::path& outputPath) const;

private:
    std::mutex _mutex;
    std::unordered_map<EmissionIdentifier, double> _emissionSums;
};

}