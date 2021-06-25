#include "emap/emissions.h"

#include "infra/exception.h"

namespace emap {

using namespace inf;

std::string_view emission_type_name(EmissionType type)
{
    switch (type) {
    case EmissionType::Historic:
        return "historic";
    case EmissionType::Future:
        return "future";
    }

    throw RuntimeError("Invalid emission type");
}

std::string_view emission_sector_type_name(EmissionSector::Type type)
{
    switch (type) {
    case EmissionSector::Type::Nfr:
        return "nfr";
    case EmissionSector::Type::Gnfr:
        return "gnfr";
    }

    throw RuntimeError("Invalid emission sector type");
}

}
