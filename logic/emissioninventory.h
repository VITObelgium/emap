#pragma once

#include "emap/emissions.h"

namespace emap {

class RunSummary;
class ScalingFactors;

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary);

EmissionInventory create_emission_inventory(const SingleEmissions& totalEmissionsNfr,
                                            const SingleEmissions& totalEmissionsNfrOlder,
                                            const SingleEmissions& totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            RunSummary& runSummary);

}
