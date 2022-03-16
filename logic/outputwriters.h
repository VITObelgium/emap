#pragma once

#include "brnoutputentry.h"
#include "datoutputentry.h"
#include "emap/pollutant.h"

#include "infra/filesystem.h"
#include "infra/span.h"

#include <vector>

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
    BrnOutputWriter(const fs::path& path, OpenMode mode, size_t startIndex);

    void write_header();
    void append_entries(std::span<const BrnOutputEntry> entries);
    size_t current_index() const noexcept;

private:
    inf::file::Handle _fp;
    size_t _index = 1;
};

void write_brn_output(std::span<const BrnOutputEntry> entries, const fs::path& path);

void write_dat_header(const fs::path& path, const std::vector<std::string>& sectors);
void write_dat_output(const fs::path& path, std::span<const DatOutputEntry> entries);
void write_dat_output(const fs::path& path, std::span<const DatPointSourceOutputEntry> entries, const std::vector<std::string>& pollutants);

}