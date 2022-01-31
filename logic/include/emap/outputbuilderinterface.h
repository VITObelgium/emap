#pragma once

#include <cinttypes>

namespace emap {

class EmissionEntry;
struct EmissionIdentifier;

class IOutputBuilder
{
public:
    virtual ~IOutputBuilder() = default;

    virtual void add_point_output_entry(const EmissionEntry& emission)                                         = 0;
    virtual void add_diffuse_output_entry(const EmissionIdentifier& id, int64_t x, int64_t y, double emission) = 0;
};

}