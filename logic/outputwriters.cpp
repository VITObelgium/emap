#include "outputwriters.h"
#include "emap/runconfiguration.h"
#include "emapconfig.h"

#include <fmt/compile.h>
#include <fmt/core.h>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

BrnOutputWriter::BrnOutputWriter(const fs::path& path, OpenMode mode)
{
    fs::create_directories(path.parent_path());

    if (mode == OpenMode::Append) {
        _fp.open(path, "at");
    } else {
        _fp.open(path, "wt");
    }

    if (!_fp.is_open()) {
        throw RuntimeError("Failed to create brn output file: {}", path);
    }
}

void BrnOutputWriter::append_entries(std::span<const BrnOutputEntry> entries)
{
    for (const auto& entry : entries) {
        //                           index     x     y      q      hc       h     d       s    dv   cat  area    sd comp     temp     flow
        fmt::print(_fp, FMT_COMPILE("{:>6d}{:>8d}{:>8d}{:>14.7e}{:>7.2f}{:>6.1f}{:>7d}{:>6.1f}{:>4d}{:>4d}{:>4d}{:>4d}{:>5}{:>12.3f}{:>12.3f}\n"),
                   entry.ssn,
                   entry.x_m,
                   entry.y_m,
                   entry.q_gs,
                   entry.hc_MW,
                   entry.h_m,
                   entry.d_m,
                   entry.s_m,
                   entry.dv,
                   entry.cat,
                   entry.area,
                   entry.sd,
                   entry.comp,
                   entry.temp,
                   entry.flow);
    }
}

void BrnOutputWriter::write_header()
{
    fmt::print(_fp, "   ssn    x(m)    y(m)        q(g/s) hc(MW)  h(m)   d(m)  s(m) dv cat area  sd comp        temp        flow Emap: v" EMAP_VERSION "\n");
}

void write_brn_output(std::span<const BrnOutputEntry> entries, const fs::path& path)
{
    BrnOutputWriter writer(path, BrnOutputWriter::OpenMode::Replace);
    writer.append_entries(entries);
}

void write_dat_header(const fs::path& path, const std::vector<std::string>& sectors)
{
    file::write_as_text(path, fmt::format("country row col {}\n", str::join(sectors, ' ')));
}

void write_dat_output(const fs::path& path, std::span<const DatOutputEntry> entries)
{
    file::Handle fp(path, "wt");

    for (const auto& entry : entries) {
        fmt::print(fp, FMT_COMPILE("{:>4}{:>5}{:>5} {}\n"), entry.countryCode, entry.cell.c, entry.cell.r, str::join(entry.emissions, " "sv, [](double emission) {
                       return fmt::format("{:>10.3e}", emission);
                   }));
    }
}

void write_dat_output(const fs::path& path, std::span<const DatPointSourceOutputEntry> entries, const std::vector<std::string>& pollutants)
{
    file::Handle fp(path, "wt");

    fmt::print(fp, FMT_COMPILE("PIG      Long       Lat Country snap      temp     Vel  Height    Diam {}\n"), str::join(pollutants, " ", [](const std::string& pollutant) {
                   return fmt::format("{:>9}", pollutant);
               }));

    for (const auto& entry : entries) {
        //                           PIG     Long    Lat Country snap    temp     Vel  Height    Diam
        fmt::print(fp, FMT_COMPILE("{:>3}{:>10.4f}{:>10.4f}{:>8}{:>5}{:>10.3f}{:>8.3f}{:>8.3f}{:>8.3f} {}\n"), entry.pig, entry.coordinate.longitude, entry.coordinate.latitude, entry.countryCode, entry.sectorId, entry.temperature, entry.velocity, entry.height, entry.diameter, str::join(entry.emissions, " ", [](double emission) {
                       return fmt::format("{:>9.3f}", emission);
                   }));
    }
}

}