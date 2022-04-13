#include "outputreaders.h"

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

std::vector<BrnOutputEntry> read_brn_output(const fs::path& path)
{
    const auto fileContents = file::read_as_text(path);
    auto lines              = str::split_view(fileContents, '\n');

    auto start = lines.begin();

    if (str::starts_with(lines.front(), "   ssn")) {
        ++start; // skip the header
    }

    std::vector<BrnOutputEntry> result;
    result.reserve(lines.size());

    //"   ssn    x(m)    y(m)   q(g/s)     hc(MW)  h(m)   d(m)  s(m)  dv cat area  sd  comp temp flow Emap: v..");

    //                             index     x     y      q      hc       h     d       s    dv   cat  area    sd comp     temp     flow
    // fmt::print(_fp, FMT_COMPILE("{:>6d}{:>8d}{:>8d}{:>13e}{:>7.3f}{:>6.1f}{:>7d}{:>6.1f}{:>4d}{:>4d}{:>4d}{:>4d}{:>5}{:>12.3f}{:>12.3f}\n"),

    std::for_each(start, lines.end(), [&result](std::string_view line) {
        if (line.empty()) {
            return;
        }

        if (line.size() < 106) {
            throw RuntimeError("Invalid BRN line length: {}", line.size());
        }

        BrnOutputEntry entry;
        size_t offset = 0;

        entry.ssn = str::to_int32_value(line.substr(offset, 6));
        offset += 6;
        entry.x_m = str::to_int64_value(line.substr(offset, 8));
        offset += 8;
        entry.y_m = str::to_int64_value(line.substr(offset, 8));
        offset += 8;
        entry.q_gs = str::to_double_value(line.substr(offset, 13));
        offset += 13;
        entry.hc_MW = str::to_double_value(line.substr(offset, 7));
        offset += 7;
        entry.h_m = str::to_double_value(line.substr(offset, 6));
        offset += 6;
        entry.d_m = str::to_int32_value(line.substr(offset, 7));
        offset += 7;
        entry.s_m = str::to_double_value(line.substr(offset, 6));
        offset += 6;
        entry.dv = str::to_int32_value(line.substr(offset, 4));
        offset += 4;
        entry.cat = str::to_int32_value(line.substr(offset, 4));
        offset += 4;
        entry.area = str::to_int32_value(line.substr(offset, 4));
        offset += 4;
        entry.sd = str::to_int32_value(line.substr(offset, 4));
        offset += 4;
        entry.comp = str::trimmed_view(line.substr(offset, 5));
        offset += 5;
        entry.temp = str::to_double_value(line.substr(offset, 12));
        offset += 12;
        entry.flow = str::to_double_value(line.substr(offset, 12));

        result.push_back(entry);
    });

    return result;
}

}