#pragma once

#include "emap/preprocessing.h"
#include "emap/runconfiguration.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

#include <variant>

namespace emap {

struct ModelProgressInfo
{
    ModelProgressInfo(PreprocessingProgressInfo i)
    : info(i)
    {
    }

    std::variant<PreprocessingProgressInfo> info;

    std::string to_string() const
    {
        return std::visit([](const auto& info) {
            return info.to_string();
        },
                          info);
    }
};

using ModelProgress = inf::ProgressTracker<ModelProgressInfo>;

void run_model(const fs::path& runConfigPath, ModelProgress::Callback progressCb);
void run_model(const RunConfiguration& cfg, ModelProgress::Callback progressCb);

}
