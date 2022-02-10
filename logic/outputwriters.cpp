#include "outputwriters.h"
#include "emap/runconfiguration.h"
#include "emapconfig.h"

#include <fmt/compile.h>
#include <fmt/core.h>

namespace emap {

using namespace inf;

BrnOutputWriter::BrnOutputWriter(const fs::path& path)
{
    fs::create_directories(path.parent_path());

    _fp.open(path, "wt");
    if (!_fp.is_open()) {
        throw RuntimeError("Failed to create brn output file: {}", path);
    }

    write_header();
}

void BrnOutputWriter::append_entries(std::span<const BrnOutputEntry> entries)
{
    for (const auto& entry : entries) {
        //                           index     x     y        q      hc       h     d       s    dv   cat  area    sd comp     temp     flow
        fmt::print(_fp, FMT_COMPILE("{:>6d}{:>8d}{:>8d}{:>14.8g}{:>7.3f}{:>6.1f}{:>7d}{:>6.1f}{:>4d}{:>4d}{:>4d}{:>4d}{:>5}{:>12.3f}{:>12.3f}\n"),
                   _index,
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

        ++_index;
    }
}

void BrnOutputWriter::write_header()
{
    fmt::print(_fp, "   ssn    x(m)    y(m)   q(g/s)     hc(MW)  h(m)   d(m)  s(m)  dv cat area  sd  comp temp flow Emap: v" EMAP_VERSION "\n");
}

void write_brn_output(std::span<const BrnOutputEntry> entries, const fs::path& path)
{
    BrnOutputWriter writer(path);
    writer.append_entries(entries);
}

}