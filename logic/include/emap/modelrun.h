#pragma once

#include "emap/runconfiguration.h"
#include "infra/filesystem.h"
#include "infra/log.h"
#include "infra/progressinfo.h"

namespace emap {

struct ModelProgressInfo
{
    ModelProgressInfo() = default;
    ModelProgressInfo(std::string_view i)
    : info(i)
    {
    }

    std::string to_string() const
    {
        return info;
    }

    std::string info;
};

using ModelProgress = inf::ProgressTracker<ModelProgressInfo>;

int run_model(const fs::path& runConfigPath, inf::Log::Level logLevel, std::optional<int32_t> concurrency, const ModelProgress::Callback& progressCb);
int run_model(const RunConfiguration& cfg, const ModelProgress::Callback& progressCb);

}
