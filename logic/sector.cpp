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

template <typename SectorInfo>
static auto find_sector_with_name(std::string_view name, const SectorInfo& sectors)
{
    return std::find_if(sectors.begin(), sectors.end(), [name](const auto& sector) {
        return sector.name() == name;
    });
}

template <typename SectorInfo>
static auto find_sector_with_code(std::string_view code, const SectorInfo& sectors)
{
    return std::find_if(sectors.begin(), sectors.end(), [code](const auto& sector) {
        return sector.code() == code;
    });
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

int32_t EmissionSector::id() const noexcept
{
    return std::visit([](auto& sectorType) {
        return static_cast<int32_t>(sectorType.id());
    },
                      _sector);
}

bool EmissionSector::is_land_sector() const noexcept
{
    return gnfr_sector().has_land_destination();
}

// std::optional<NfrSector> EmissionSector::is_sector_override() const noexcept
//{
//     if (type() == Type::Gnfr) {
//         return {};
//     }
//
//     switch (get_nfr_sector()) {
//     case NfrSector::Nfr1A3bi_fu:
//         return NfrSector::Nfr1A3bi;
//     case NfrSector::Nfr1A3bii_fu:
//         return NfrSector::Nfr1A3bii;
//     case NfrSector::Nfr1A3biii_fu:
//         return NfrSector::Nfr1A3biii;
//     case NfrSector::Nfr1A3biv_fu:
//         return NfrSector::Nfr1A3biv;
//     case NfrSector::Nfr1A3bv_fu:
//         return NfrSector::Nfr1A3bv;
//     case NfrSector::Nfr1A3bvi_fu:
//         return NfrSector::Nfr1A3bvi;
//     case NfrSector::Nfr1A3bvii_fu:
//         return NfrSector::Nfr1A3bvii;
//     }
//
//     return {};
// }

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

SectorInventory::SectorInventory(std::vector<GnfrSector> gnfrSectors, std::vector<NfrSector> nfrSectors, InputConversions gnfrSectorConversions, InputConversions nfrSectorConversions)
: _gnfrSectors(std::move(gnfrSectors))
, _nfrSectors(std::move(nfrSectors))
, _gnfrConversions(std::move(gnfrSectorConversions))
, _nfrConversions(std::move(nfrSectorConversions))
{
}

EmissionSector SectorInventory::sector_from_string(std::string_view name) const
{
    if (auto sector = try_sector_from_string(name); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid sector name: '{}'", name);
}

EmissionSector SectorInventory::sector_from_string(EmissionSector::Type type, std::string_view name) const
{
    if (auto sector = try_sector_from_string(type, name); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid sector name: '{}'", name);
}

std::optional<EmissionSector> SectorInventory::try_sector_from_string(std::string_view name) const
{
    if (const auto gnfrSector = try_gnfr_sector_from_string(name); gnfrSector.has_value()) {
        return EmissionSector(*gnfrSector);
    } else if (const auto nfrSector = try_nfr_sector_from_string(name); nfrSector.has_value()) {
        return EmissionSector(*nfrSector);
    }

    return {};
}

std::optional<EmissionSector> SectorInventory::try_sector_from_string(EmissionSector::Type type, std::string_view name) const
{
    switch (type) {
    case EmissionSector::Type::Nfr:
        if (auto sector = try_nfr_sector_from_string(name); sector.has_value()) {
            return EmissionSector(*sector);
        }

        break;
    case EmissionSector::Type::Gnfr:
        if (auto sector = try_gnfr_sector_from_string(name); sector.has_value()) {
            return EmissionSector(*sector);
        }

        break;
    default:
        throw RuntimeError("Invalid sector type");
    }

    return {};
}

std::optional<GnfrSector> SectorInventory::try_gnfr_sector_from_string(std::string_view str) const noexcept
{
    auto gnfrCode = _gnfrConversions.lookup(str);
    if (gnfrCode.empty()) {
        gnfrCode = str; // not all valid names have to be present in the conversion table
    }

    if (auto iter = find_sector_with_code(gnfrCode, _gnfrSectors); iter != _gnfrSectors.end()) {
        return *iter;
    }

    return {};
}

std::optional<NfrSector> SectorInventory::try_nfr_sector_from_string(std::string_view str) const noexcept
{
    auto nfrCode = _nfrConversions.lookup(str);
    if (nfrCode.empty()) {
        nfrCode = str; // not all valid names have to be present in the conversion table
    }

    if (auto iter = find_sector_with_name(nfrCode, _nfrSectors); iter != _nfrSectors.end()) {
        return *iter;
    }

    return {};
}

std::optional<std::pair<NfrSector, int32_t>> SectorInventory::try_nfr_sector_with_priority_from_string(std::string_view str) const noexcept
{
    auto [nfrCode, priority] = _nfrConversions.lookup_with_priority(str);
    if (!nfrCode.empty()) {
        if (auto iter = find_sector_with_name(nfrCode, _nfrSectors); iter != _nfrSectors.end()) {
            return std::make_pair(*iter, priority);
        }
    }

    return {};
}

GnfrSector SectorInventory::gnfr_sector_from_string(std::string_view str) const
{
    if (auto sector = try_gnfr_sector_from_string(str); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid gnfr sector name: '{}'", str);
}

NfrSector SectorInventory::nfr_sector_from_string(std::string_view str) const
{
    if (auto sector = try_nfr_sector_from_string(str); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid nfr sector name: '{}'", str);
}

std::pair<NfrSector, int32_t> SectorInventory::nfr_sector_with_priority_from_string(std::string_view str) const
{
    if (auto sector = try_nfr_sector_with_priority_from_string(str); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid nfr sector name: '{}'", str);
}

GnfrSector SectorInventory::gnfr_sector_from_id(GnfrId id) const
{
    return find_in_container_required(_gnfrSectors, [id](const GnfrSector& sector) {
        return sector.id() == id;
    });
}

size_t SectorInventory::gnfr_sector_count() const noexcept
{
    return _gnfrSectors.size();
}

size_t SectorInventory::nfr_sector_count() const noexcept
{
    return _nfrSectors.size();
}

bool SectorInventory::is_ignored_sector(std::string_view str) const noexcept
{
    return str::iequals(str, "1A3ai(ii)") ||
           str::iequals(str, "1A3aii(ii)") ||
           str::iequals(str, "1A3di(i)") ||
           str::iequals(str, "1A5c") ||
           str::iequals(str, "6B") ||
           str::iequals(str, "1A3") ||
           str::iequals(str, "11A") ||
           str::iequals(str, "11B") ||
           str::iequals(str, "11C");
}

std::span<const GnfrSector> SectorInventory::gnfr_sectors() const noexcept
{
    return _gnfrSectors;
}

std::span<const NfrSector> SectorInventory::nfr_sectors() const noexcept
{
    return _nfrSectors;
}
}
