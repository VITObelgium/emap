#pragma once

#include "emap/emissions.h"
#include "emap/scalingfactors.h"
#include "infra/algo.h"
#include "infra/log.h"

#include <cassert>
#include <numeric>

namespace emap {

using namespace inf;

class RunSummary;

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary);

}
