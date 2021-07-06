#include "emap/modelrun.h"

#include "emap/configurationparser.h"
#include "emap/inputparsers.h"
#include "emap/preprocessing.h"
#include "emap/scalingfactors.h"
#include "emissioninventory.h"

#include "infra/chrono.h"
#include "infra/exception.h"
#include "infra/log.h"

#include <numeric>

namespace emap {

using namespace inf;

static fs::path throw_if_not_exists(fs::path&& path)
{
    if (!fs::is_regular_file(path)) {
        throw RuntimeError("File does not exist: {}", path);
    }

    return std::move(path);
}

void run_model(const fs::path& runConfigPath, ModelProgress::Callback progressCb)
{
    Log::debug("Process configuration: {}", runConfigPath);

    // Check if there is a preprocessing step defined
    run_preprocessing(runConfigPath, [&progressCb](const PreprocessingProgress::Status& status) {
        if (progressCb) {
            return progressCb(ModelProgress::Status(status.current(), status.total(), status.payload()));
        }

        return ProgressStatusResult::Continue;
    });

    return run_model(parse_run_configuration_file(runConfigPath), progressCb);
}

void run_model(const RunConfiguration& cfg, ModelProgress::Callback /*progressCb*/)
{
    const auto pointSource         = parse_emissions(throw_if_not_exists(cfg.point_source_emissions_path()));
    const auto nfrTotalEmissions   = parse_emissions(throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Nfr)));
    const auto gnfrTotalEmissions  = parse_emissions(throw_if_not_exists(cfg.total_emissions_path(EmissionSector::Type::Gnfr)));
    const auto scalingsDiffuse     = parse_scaling_factors(throw_if_not_exists(cfg.diffuse_scalings_path()));
    const auto scalingsPointSource = parse_scaling_factors(throw_if_not_exists(cfg.point_source_scalings_path()));

    Log::debug("Generate emission inventory");
    chrono::DurationRecorder dur;
    const auto inventory = create_emission_inventory(nfrTotalEmissions, pointSource, scalingsDiffuse, scalingsPointSource);
    Log::debug("Generate emission inventory took {}", dur.elapsed_time_string());
}
}
