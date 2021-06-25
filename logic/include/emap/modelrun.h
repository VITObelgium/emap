#pragma once

#include "emap/runconfiguration.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

namespace emap {

struct ModelProgressInfo
{
    std::string current;
};

using ModelProgress = inf::ProgressTracker<ModelProgressInfo>;

void run_model(const fs::path& runConfigPath, ModelProgress::Callback progressCb);
void run_model(const RunConfiguration& cfg, ModelProgress::Callback progressCb);

}
