#pragma once

#include "brnoutputentry.h"
#include "infra/filesystem.h"
#include "infra/span.h"

#include "emap/pollutant.h"

namespace emap {

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