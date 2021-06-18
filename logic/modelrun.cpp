#include "emap/modelrun.h"

#include "emap/runconfigurationparser.h"

namespace emap {

void run_model(const fs::path& runConfigPath, ModelProgress::Callback progressCb)
{
    return run_model(parse_run_configuration(runConfigPath), progressCb);
}

void run_model(const RunConfiguration& cfg, ModelProgress::Callback progressCb)
{
    const auto& dataPath = cfg.data_root();
}

}
