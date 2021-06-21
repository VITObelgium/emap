#include "emap/modelrun.h"

#include "emap/runconfigurationparser.h"
#include "infra/exception.h"
#include "infra/log.h"
#include <filesystem>

namespace emap {

using namespace inf;

static fs::path throw_if_not_exists(const fs::path&& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return path;
}

void run_model(const fs::path& runConfigPath, ModelProgress::Callback progressCb)
{
    Log::debug("Process configuration: {}", runConfigPath);
    return run_model(parse_run_configuration(runConfigPath), progressCb);
}

void run_model(const RunConfiguration& cfg, ModelProgress::Callback /*progressCb*/)
{
    const auto pointSourcePath        = throw_if_not_exists(cfg.point_source_emissions_path());
    const auto nfrTotalEmissionsPath  = throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Nfr));
    const auto gnfrTotalEmissionsPath = throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Gnfr));
    const auto scalingsDiffuse        = throw_if_not_exists(cfg.diffuse_scalings_path());
    const auto scalingsPointSource    = throw_if_not_exists(cfg.point_source_scalings_path());

    Log::debug("Point sources:         {}", pointSourcePath);
    Log::debug("Nfr total emissions:   {}", nfrTotalEmissionsPath);
    Log::debug("Gnfr total emissions:  {}", gnfrTotalEmissionsPath);
    Log::debug("Diffuse scalings:      {}", scalingsDiffuse);
    Log::debug("Point source scalings: {}", scalingsPointSource);
}

}
