#include "infra/cliprogressbar.h"
#include "infra/filesystem.h"
#include "infra/gdal.h"
#include "infra/gdallog.h"
#include "infra/log.h"

#include "infra/progressinfo.h"

#include <cstdlib>
#include <fmt/ostream.h>
#include <locale>
#include <lyra/lyra.hpp>
#include <optional>

#ifdef WIN32
#include <windows.h>
#endif

using inf::Log;

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
        bool showHelp     = false;
        bool showProgress = false;
        bool consoleLog   = false;
        std::optional<int32_t> concurrency;
    } options;

    auto cli = lyra::help(options.showHelp) |
               lyra::opt(options.consoleLog)["-l"]["--log"]("Print logging on the console") |
               lyra::opt(options.showProgress)["--progress"]("Show progress bar on the console") |
               lyra::opt(options.concurrency, "number")["--concurrency"]("Number of cores to use");

    cli.parse(lyra::args(argc, argv));
    if (options.showHelp) {
        fmt::print("{}\n", cli);
        return EXIT_SUCCESS;
    }

    try {
        inf::Log::add_console_sink(inf::Log::Colored::On);
        inf::LogRegistration logReg("e-map");
        inf::gdal::Registration reg;
        inf::gdal::set_log_handler();

#ifdef NDEBUG
        inf::Log::set_level(inf::Log::Level::Info);
#else
        inf::Log::set_level(inf::Log::Level::Debug);
#endif

    } catch (const std::exception& e) {
        fmt::print("{}\n", e.what());
        inf::Log::error(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
