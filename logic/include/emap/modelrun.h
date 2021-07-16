#pragma once

#include "emap/preprocessing.h"
#include "emap/runconfiguration.h"
#include "infra/filesystem.h"
#include "infra/progressinfo.h"

#include <variant>

namespace emap {

struct ModelRunProgressInfo
{
    std::string to_string() const
    {
        return {};
    }
};

struct ModelProgressInfo
{
    ModelProgressInfo() = default;
    ModelProgressInfo(ModelRunProgressInfo i)
    : info(i)
    {
    }

    ModelProgressInfo(PreprocessingProgressInfo i)
    : info(i)
    {
    }

    std::string to_string() const
    {
        return std::visit([](const auto& info) {
            return info.to_string();
        },
                          info);
    }

    std::variant<PreprocessingProgressInfo, ModelRunProgressInfo> info;
};

using ModelProgress = inf::ProgressTracker<ModelProgressInfo>;

void run_model(const fs::path& runConfigPath, const ModelProgress::Callback& progressCb);
void run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb);

}
