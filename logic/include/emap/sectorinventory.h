#pragma once

#include "emap/sector.h"
#include "infra/span.h"

#include <string_view>
#include <type_safe/strong_typedef.hpp>
#include <unordered_map>
#include <vector>

namespace emap {

class SectorInventory
{
public:
    SectorInventory(std::vector<GnfrSector> gnfrSectors,
                    std::vector<NfrSector> nfrSectors,
                    InputConversions gnfrSectorConversions,
                    InputConversions nfrSectorConversions);

    void set_output_mapping(std::unordered_map<NfrId, std::string> mapping);

    std::string map_nfr_to_output_name(const NfrSector& nfr) const;

    EmissionSector sector_from_string(std::string_view name) const;
    EmissionSector sector_from_string(EmissionSector::Type type, std::string_view name) const;
    std::pair<EmissionSector, int32_t> sector_with_priority_from_string(EmissionSector::Type type, std::string_view str) const;

    std::optional<EmissionSector> try_sector_from_string(std::string_view name) const;
    std::optional<EmissionSector> try_sector_from_string(EmissionSector::Type type, std::string_view name) const;

    GnfrSector gnfr_sector_from_string(std::string_view str) const;
    GnfrSector gnfr_sector_from_code_string(std::string_view str) const;
    NfrSector nfr_sector_from_string(std::string_view str) const;
    std::pair<GnfrSector, int32_t> gnfr_sector_with_priority_from_string(std::string_view str) const;
    std::pair<NfrSector, int32_t> nfr_sector_with_priority_from_string(std::string_view str) const;

    std::optional<GnfrSector> try_gnfr_sector_from_string(std::string_view str) const noexcept;
    std::optional<NfrSector> try_nfr_sector_from_string(std::string_view str) const noexcept;

    GnfrSector gnfr_sector_from_id(GnfrId id) const;

    size_t gnfr_sector_count() const noexcept;
    size_t nfr_sector_count() const noexcept;

    bool is_ignored_sector(std::string_view str) const noexcept;

    std::span<const GnfrSector> gnfr_sectors() const noexcept;
    std::span<const NfrSector> nfr_sectors() const noexcept;

    std::vector<NfrSector> nfr_sectors_in_gnfr(GnfrId gnfr) const;

private:
    std::optional<std::pair<GnfrSector, int32_t>> try_gnfr_sector_with_priority_from_string(std::string_view str) const noexcept;
    std::optional<std::pair<NfrSector, int32_t>> try_nfr_sector_with_priority_from_string(std::string_view str) const noexcept;

    // List of sectors used by the emap model
    std::vector<GnfrSector> _gnfrSectors;
    std::vector<NfrSector> _nfrSectors;

    // Mapping from all possible sector names in reported files to the emap names
    InputConversions _gnfrConversions;
    InputConversions _nfrConversions;

    std::unordered_map<NfrId, std::string> _outputMapping;
};

}
