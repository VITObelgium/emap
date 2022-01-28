#pragma once

#include "emap/emissions.h"

namespace emap {

class RunSummary;
class ScalingFactors;

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary);

}
