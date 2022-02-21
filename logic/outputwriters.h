#pragma once

#include "brnoutputentry.h"
#include "infra/filesystem.h"
#include "infra/span.h"

#include "emap/pollutant.h"

namespace emap {

class BrnOutputWriter
{
public:
    enum class OpenMode
    {
        Replace,
        Append,
    };

    BrnOutputWriter(const fs::path& path, OpenMode mode);

    void write_header();
    void append_entries(std::span<const BrnOutputEntry> entries);

private:
    inf::file::Handle _fp;
    size_t _index = 1;
};

void write_brn_output(std::span<const BrnOutputEntry> entries, const fs::path& path);

}