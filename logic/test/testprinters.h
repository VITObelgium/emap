#pragma once

#include "emap/emissions.h"

#include <doctest/doctest.h>

namespace emap {

inline doctest::String toString(const EmissionSector& sector)
{
    return doctest::String(sector.name().data(), static_cast<int>(sector.name().size()));
}

inline doctest::String toString(const EmissionSector::Type& type)
{
    if (type == EmissionSector::Type::Gnfr) {
        return "Gnfr";
    }

    if (type == EmissionSector::Type::Nfr) {
        return "Nfr";
    }

    return "";
}

}
