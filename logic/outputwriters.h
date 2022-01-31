#pragma once

#include "infra/filesystem.h"
#include "infra/span.h"

#include "emap/pollutant.h"

namespace emap {

struct BrnOutputEntry
{
    int64_t x    = 0;
    int64_t y    = 0;
    double q     = 0.0;
    double hc    = 0.0;
    double h     = 0.0;
    int32_t d    = 0;
    double s     = 0.0;
    int32_t dv   = 0;
    int32_t cat  = 0;
    int32_t area = 0;
    int32_t sd   = 0;
    Pollutant comp;
    double temp = 9999.0;
    double flow = 9999.0;
};

class BrnOutputWriter
{
public:
    BrnOutputWriter(const fs::path& path);

    void append_entries(std::span<const BrnOutputEntry> entries);

private:
    void write_header();

    inf::file::Handle _fp;
    size_t _index = 1;
};

void write_brn_output(std::span<const BrnOutputEntry> entries, const fs::path& path);

}