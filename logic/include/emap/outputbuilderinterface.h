#pragma once

#include "emap/runconfiguration.h"
#include "emap/sectorparameterconfig.h"

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

    // Pollutant calculation finished, results can be flushed to save on memory
    virtual void flush_pollutant(const Pollutant& pol, WriteMode mode) = 0;

    // flush all intermediate results to disk
    virtual void flush(WriteMode mode) = 0;
};

}