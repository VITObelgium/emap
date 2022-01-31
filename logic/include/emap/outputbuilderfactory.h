#pragma once

#include "emap/outputbuilderinterface.h"

#include <memory>

namespace emap {

class EmissionId;
class RunConfiguration;

std::unique_ptr<IOutputBuilder> make_output_builder(const RunConfiguration& cfg);

}