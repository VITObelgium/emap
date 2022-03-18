#include "emap/sectorinventory.h"

#include "infra/algo.h"
#include "infra/exception.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace emap {

using namespace inf;
using namespace std::string_view_literals;

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

SectorInventory::SectorInventory(std::vector<GnfrSector> gnfrSectors,
                                 std::vector<NfrSector> nfrSectors,
                                 InputConversions gnfrSectorConversions,
                                 InputConversions nfrSectorConversions,
                                 std::vector<std::string> ignoredGnfrSectors,
                                 std::vector<std::string> ignoredNfrSectors)
: _gnfrSectors(std::move(gnfrSectors))
, _nfrSectors(std::move(nfrSectors))
, _gnfrConversions(std::move(gnfrSectorConversions))
, _nfrConversions(std::move(nfrSectorConversions))
, _ignoredGnfrSectors(std::move(ignoredGnfrSectors))
, _ignoredNfrSectors(std::move(ignoredNfrSectors))
{
}

void SectorInventory::set_output_mapping(std::unordered_map<NfrId, std::string> mapping)
{
    _outputMapping = std::move(mapping);
}

std::string SectorInventory::map_nfr_to_output_name(const NfrSector& nfr) const
{
    try {
        return _outputMapping.at(nfr.id());
    } catch (const std::out_of_range&) {
        throw RuntimeError("No mapping defined for nfr sector: {}", nfr.name());
    }
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

std::pair<EmissionSector, int32_t> SectorInventory::sector_with_priority_from_string(EmissionSector::Type type, std::string_view str) const
{
    if (type == EmissionSector::Type::Gnfr) {
        auto [sector, priority] = gnfr_sector_with_priority_from_string(str);
        return {EmissionSector(sector), priority};
    } else if (type == EmissionSector::Type::Nfr) {
        auto [sector, priority] = nfr_sector_with_priority_from_string(str);
        return {EmissionSector(sector), priority};
    }

    throw RuntimeError("Invalid sector type");
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

std::optional<std::pair<GnfrSector, int32_t>> SectorInventory::try_gnfr_sector_with_priority_from_string(std::string_view str) const noexcept
{
    auto [gnfrCode, priority] = _gnfrConversions.lookup_with_priority(str);
    if (!gnfrCode.empty()) {
        if (auto iter = find_sector_with_code(gnfrCode, _gnfrSectors); iter != _gnfrSectors.end()) {
            return std::make_pair(*iter, priority);
        }
    }

    return {};
}

std::optional<std::pair<NfrSector, int32_t>> SectorInventory::try_nfr_sector_with_priority_from_string(std::string_view str) const noexcept
{
    auto [nfrCode, priority] = _nfrConversions.lookup_with_priority(str);
    if (!nfrCode.empty()) {
        if (auto iter = find_sector_with_code(nfrCode, _nfrSectors); iter != _nfrSectors.end()) {
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

GnfrSector SectorInventory::gnfr_sector_from_code_string(std::string_view str) const
{
    if (auto iter = find_sector_with_code(str, _gnfrSectors); iter != _gnfrSectors.end()) {
        return *iter;
    }

    throw RuntimeError("Invalid gnfr sector code: '{}'", str);
}

NfrSector SectorInventory::nfr_sector_from_string(std::string_view str) const
{
    if (auto sector = try_nfr_sector_from_string(str); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid nfr sector name: '{}'", str);
}

std::pair<GnfrSector, int32_t> SectorInventory::gnfr_sector_with_priority_from_string(std::string_view str) const
{
    if (auto sector = try_gnfr_sector_with_priority_from_string(str); sector.has_value()) {
        return *sector;
    }

    throw RuntimeError("Invalid gnfr sector name: '{}'", str);
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

bool SectorInventory::is_ignored_nfr_sector(std::string_view str) const noexcept
{
    return std::any_of(_ignoredNfrSectors.begin(), _ignoredNfrSectors.end(), [=](const std::string& ign) {
        return str::iequals(ign, str);
    });
}

bool SectorInventory::is_ignored_gnfr_sector(std::string_view str) const noexcept
{
    return std::any_of(_ignoredGnfrSectors.begin(), _ignoredGnfrSectors.end(), [=](const std::string& ign) {
        return str::iequals(ign, str);
    });
}

bool SectorInventory::is_ignored_sector(EmissionSector::Type type, std::string_view str) const noexcept
{
    switch (type) {
    case EmissionSector::Type::Nfr:
        return is_ignored_nfr_sector(str);
    case EmissionSector::Type::Gnfr:
        return is_ignored_gnfr_sector(str);
    }

    return false;
}

std::span<const GnfrSector> SectorInventory::gnfr_sectors() const noexcept
{
    return _gnfrSectors;
}

std::span<const NfrSector> SectorInventory::nfr_sectors() const noexcept
{
    return _nfrSectors;
}

std::vector<NfrSector> SectorInventory::nfr_sectors_in_gnfr(GnfrId gnfr) const
{
    std::vector<NfrSector> result;

    for (const auto& nfr : nfr_sectors()) {
        if (nfr.gnfr().id() == gnfr) {
            result.push_back(nfr);
        }
    }

    return result;
}

}
