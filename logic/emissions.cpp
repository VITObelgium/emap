#include "emap/emissions.h"

namespace emap {

void Emissions::add_emission(EmissionInfo&& info)
{
    _emissions.push_back(std::move(info));
}

size_t Emissions::size() const noexcept
{
    return _emissions.size();
}

}