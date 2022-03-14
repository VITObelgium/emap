#include "emap/sector.h"

#include "enuminfo.h"
#include "infra/algo.h"
#include "infra/enumutils.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

GnfrSector::GnfrSector(std::string_view name, GnfrId id, std::string_view code, std::string_view description, EmissionDestination destination)
: _id(id)
, _destination(destination)
, _code(code)
, _name(name)
, _description(description)
{
}

NfrSector::NfrSector(std::string_view name, NfrId id, GnfrSector gnfr, std::string_view description, EmissionDestination destination)
: _id(id)
, _destination(destination)
, _gnfr(gnfr)
, _name(name)
, _description(description)
{
}

EmissionSector::EmissionSector(GnfrSector sector)
: _sector(sector)
{
}

EmissionSector::EmissionSector(NfrSector sector)
: _sector(sector)
{
}

EmissionSector::Type EmissionSector::type() const
{
    if (std::holds_alternative<GnfrSector>(_sector)) {
        return Type::Gnfr;
    }

    if (std::holds_alternative<NfrSector>(_sector)) {
        return Type::Nfr;
    }

    assert(false);
    throw std::logic_error("Sector not properly initialized");
}

std::string_view EmissionSector::name() const noexcept
{
    assert(!_sector.valueless_by_exception());

    if (_sector.valueless_by_exception()) {
        return "unknown";
    }

    return std::visit([](auto& sectorType) {
        return sectorType.name();
    },
                      _sector);
}

std::string_view EmissionSector::description() const noexcept
{
    assert(!_sector.valueless_by_exception());

    if (_sector.valueless_by_exception()) {
        return "unknown";
    }

    return std::visit([](auto& sectorType) {
        return sectorType.description();
    },
                      _sector);
}

std::string_view EmissionSector::gnfr_name() const noexcept
{
    return gnfr_sector().name();
}

const GnfrSector& EmissionSector::gnfr_sector() const noexcept
{
    if (type() == Type::Gnfr) {
        return get_gnfr_sector();
    }

    return get_nfr_sector().gnfr();
}

const NfrSector& EmissionSector::nfr_sector() const
{
    if (type() == Type::Gnfr) {
        throw RuntimeError("Not an nfr sector");
    }

    return get_nfr_sector();
}

int32_t EmissionSector::id() const noexcept
{
    return std::visit([](auto& sectorType) {
        return static_cast<int32_t>(sectorType.id());
    },
                      _sector);
}

bool EmissionSector::is_land_sector() const noexcept
{
    return std::visit([](auto& sectorType) {
        return sectorType.has_land_destination();
    },
                      _sector);
}

const NfrSector& EmissionSector::get_nfr_sector() const noexcept
{
    assert(type() == Type::Nfr);
    return std::get<NfrSector>(_sector);
}

const GnfrSector& EmissionSector::get_gnfr_sector() const noexcept
{
    assert(type() == Type::Gnfr);
    return std::get<GnfrSector>(_sector);
}

}
