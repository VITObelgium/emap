#pragma once

#include "emap/emissions.h"

namespace emap {

class RunSummary;
class ScalingFactors;
class RunConfiguration;

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary);

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsNfrOlder,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& diffuseScalings,
                                            const ScalingFactors& pointScalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary);

}
