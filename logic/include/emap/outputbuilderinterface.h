#pragma once

#include "emap/runconfiguration.h"

#include <cinttypes>
#include <unordered_map>

namespace emap {

class EmissionEntry;
struct EmissionIdentifier;

class IOutputBuilder
{
public:
    enum class WriteMode
    {
        Create,
        Append,
    };

    virtual ~IOutputBuilder() = default;

    virtual void add_point_output_entry(const EmissionEntry& emission)                                                                 = 0;
    virtual void add_diffuse_output_entry(const EmissionIdentifier& id, inf::Point<int64_t> loc, double emission, int32_t cellSizeInM) = 0;
    virtual void write_to_disk(const RunConfiguration& cfg, WriteMode mode)                                                            = 0;
};

}