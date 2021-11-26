#include "infra/cliprogressbar.h"
#include "infra/filesystem.h"
#include "infra/gdal.h"
#include "infra/gdallog.h"
#include "infra/log.h"
#include "infra/progressinfo.h"

#include "emap/gridprocessing.h"
#include "emap/modelrun.h"

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

    struct Cli
    {
        bool showHelp   = false;
        bool noProgress = false;
        bool consoleLog = false;
        std::string preprocessPath;
        std::string config;
        int32_t logLevel = 2;
        std::optional<int32_t> concurrency;
    } options;

    auto cli = lyra::help(options.showHelp) |
               lyra::opt(options.consoleLog)["-l"]["--log"]("Print logging on the console") |
               lyra::opt(options.logLevel, "number")["--log-level"]("Log level when logging is enabled [1 (debug) - 5 (critical)] (default=2)") |
               lyra::opt(options.noProgress)["--no-progress"]("Suppress progress info on the console") |
               lyra::opt(options.concurrency, "number")["--concurrency"]("Number of cores to use") |
               lyra::opt(options.config, "path")["-c"]["--config"]("The e-map run configuration").required();

    cli.parse(lyra::args(argc, argv));
    if (options.showHelp) {
        fmt::print("{}\n", cli);
        return EXIT_SUCCESS;
    }

    try {
        inf::gdal::RegistrationConfig gdalCfg;
        gdalCfg.projdbPath = fs::u8path(argv[0]).parent_path();
        inf::gdal::Registration reg(gdalCfg);
        std::unique_ptr<inf::LogRegistration> logReg;

        if (options.consoleLog) {
            inf::Log::add_console_sink(inf::Log::Colored::On);
            inf::gdal::set_log_handler();
            logReg = std::make_unique<inf::LogRegistration>("e-map");
            inf::Log::set_level(log_level_from_value(options.logLevel));
        }

        std::unique_ptr<inf::ProgressBar> progressBar;
        if (!options.noProgress) {
            progressBar = std::make_unique<inf::ProgressBar>(60);
        }

        emap::run_model(fs::u8path(options.config), [&](const emap::ModelProgress::Status& info) {
            if (progressBar) {
                progressBar->set_progress(info.progress());
                progressBar->set_postfix_text(info.payload().to_string());
            }
            return inf::ProgressStatusResult::Continue;
        });

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        fmt::print(fmt::fg(fmt::color::red), "{}\n", e.what());
        inf::Log::error(e.what());
        return EXIT_FAILURE;
    }
}
