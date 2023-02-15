#include "infra/cliprogressbar.h"
#include "infra/filesystem.h"
#include "infra/gdal.h"
#include "infra/gdallog.h"
#include "infra/log.h"
#include "infra/progressinfo.h"

#include "emap/debugtools.h"
#include "emap/gridprocessing.h"
#include "emap/modelrun.h"
#include "emapconfig.h"

#include <cstdlib>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/ostream.h>
#include <locale>
#include <lyra/lyra.hpp>
#include <optional>

#ifdef WIN32
#include <windows.h>
#endif

using inf::Log;

static inf::Log::Level log_level_from_value(int32_t value)
{
    switch (value) {
    case 1:
        return Log::Level::Debug;
    case 2:
        return Log::Level::Info;
    case 3:
        return Log::Level::Warning;
    case 4:
        return Log::Level::Error;
    case 5:
        return Log::Level::Critical;
    default:
        throw inf::RuntimeError("Invalid log level specified '{}': value must be in range [1-5]", value);
    }
}

int main(int argc, char** argv)
{
#ifdef WIN32
    // make sure we can print utf8 characters in the windows console
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::locale::global(std::locale::classic());
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN ", "TRUE");
    CPLSetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");

    struct Cli
    {
        bool showHelp    = false;
        bool showVersion = false;
        bool noProgress  = false;
        bool consoleLog  = false;
        bool debugGrids  = false;
        std::string preprocessPath;
        std::string config;
        int32_t logLevel = 1;
        std::optional<int32_t> concurrency;
    } options;

    auto cli = lyra::help(options.showHelp) |
               lyra::opt(options.showVersion)["-v"]["--version"]("Show version information") |
               lyra::opt(options.consoleLog)["-l"]["--log"]("Print logging on the console") |
               lyra::opt(options.logLevel, "number")["--log-level"]("Log level when logging is enabled [1 (debug) - 5 (critical)] (default=2)") |
               lyra::opt(options.noProgress)["--no-progress"]("Suppress progress info on the console") |
               lyra::opt(options.concurrency, "number")["--concurrency"]("Number of cores to use (default=all)") |
               lyra::opt(options.debugGrids)["-d"]["--debug"]("Dumps internal grid usages") |
               lyra::opt(options.config, "path")["-c"]["--config"]("The e-map run configuration").required();

    if (argc == 2 && fs::is_regular_file(fs::u8path(argv[1]))) {
        // simplified cli invocation, assume argument is config file
        options.config = argv[1];
    } else {
        cli.parse(lyra::args(argc, argv));
        if (options.showHelp) {
            fmt::print("{}\n", fmt::streamed(cli));
            return EXIT_SUCCESS;
        } else if (options.showVersion) {
            fmt::print("E-MAP {} ({})", EMAP_VERSION, EMAP_COMMIT_HASH);
            return EXIT_SUCCESS;
        }
    }

    try {
        inf::gdal::RegistrationConfig gdalCfg;
        gdalCfg.projdbPath = fs::u8path(argv[0]).parent_path() / "data";
        inf::gdal::Registration reg(gdalCfg);
        inf::gdal::set_log_handler(inf::Log::Level::Debug);

        if (options.consoleLog || options.debugGrids) {
            inf::Log::add_console_sink(inf::Log::Colored::On);
        }

        std::unique_ptr<inf::ProgressBar> progressBar;
        if (!options.noProgress) {
            progressBar = std::make_unique<inf::ProgressBar>(60);
        }

        if (options.debugGrids) {
            return emap::debug_grids(fs::u8path(options.config), log_level_from_value(options.logLevel));
        } else {
            return emap::run_model(
                fs::u8path(options.config), log_level_from_value(options.logLevel), options.concurrency, [&](const emap::ModelProgress::Status& info) {
                    if (progressBar) {
                        progressBar->set_progress(info.progress());
                        progressBar->set_postfix_text(info.payload().to_string());
                    }
                    return inf::ProgressStatusResult::Continue;
                });
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        Log::error(e.what());
        if (options.noProgress) {
            fmt::print("{}\n", e.what());
        } else {
            fmt::print(fmt::fg(fmt::color::red), "{}\n", e.what());
        }
        return EXIT_FAILURE;
    }
}
