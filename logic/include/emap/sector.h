#pragma once

#include "emap/emissiondestination.h"
#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <span>
#include <string_view>
#include <type_safe/strong_typedef.hpp>
#include <unordered_set>
#include <variant>

namespace emap {

class SectorInventory;

struct GnfrId : type_safe::strong_typedef<GnfrId, int64_t>,
                type_safe::strong_typedef_op::equality_comparison<GnfrId>,
                type_safe::strong_typedef_op::relational_comparison<GnfrId>
{
    using strong_typedef::strong_typedef;
};

struct NfrId : type_safe::strong_typedef<NfrId, int64_t>,
               type_safe::strong_typedef_op::equality_comparison<NfrId>,
               type_safe::strong_typedef_op::relational_comparison<NfrId>
{
    using strong_typedef::strong_typedef;
};

class GnfrSector
{
public:
    GnfrSector() noexcept
    {
    }

    GnfrSector(std::string_view name, GnfrId id, std::string_view code, std::string_view description, EmissionDestination destination);

    std::string_view code() const noexcept
    {
        return _code;
    }

    std::string_view name() const noexcept
    {
        return _name;
    }

    std::string_view description() const noexcept
    {
        return _description;
    }

    GnfrId id() const noexcept
    {
        return _id;
    }

    bool has_land_destination() const noexcept
    {
        return _destination == EmissionDestination::Land;
    }

    bool operator==(const GnfrSector& other) const noexcept
    {
        return id() == other.id();
    }

    bool operator<(const GnfrSector& other) const noexcept
    {
        return id() < other.id();
    }

private:
    GnfrId _id;
    EmissionDestination _destination = EmissionDestination::Invalid;
    std::string _code;
    std::string _name;
    std::string _description;
};

class NfrSector
{
public:
    NfrSector() noexcept
    {
    }

    NfrSector(std::string_view name, NfrId id, GnfrSector gnfr, std::string_view description, EmissionDestination destination);

    std::string_view name() const noexcept
    {
        return _name;
    }

    std::string_view code() const noexcept
    {
        return _name;
    }

    std::string_view description() const noexcept
    {
        return _description;
    }

    NfrId id() const noexcept
    {
        return _id;
    }

    const GnfrSector& gnfr() const noexcept
    {
        return _gnfr;
    }

    bool operator==(const NfrSector& other) const noexcept
    {
        return id() == other.id();
    }

    bool operator<(const NfrSector& other) const noexcept
    {
        return id() < other.id();
    }

private:
    NfrId _id;
    EmissionDestination _destination = EmissionDestination::Invalid;
    GnfrSector _gnfr;
    std::string _name;
    std::string _description;
};

class EmissionSector
{
public:
    enum class Type
    {
        Nfr,
        Gnfr,
    };

    EmissionSector() noexcept = default;
    explicit EmissionSector(GnfrSector sector);
    explicit EmissionSector(NfrSector sector);

    Type type() const;
    std::string_view name() const noexcept;
    std::string_view description() const noexcept;
    /*! If it is a gnfr sector: returns the name
     *  If it is s nfr sector: returns the corresponding gnfr sector name
     */
    std::string_view gnfr_name() const noexcept;

    /*! If it is a gnfr sector: returns it
     *  If it is s nfr sector: returns the corresponding gnfr sector
     */
    const GnfrSector& gnfr_sector() const noexcept;

    bool is_land_sector() const noexcept;
    /* Returns the nfr sector this sector overrides if it is applicable */
    // std::optional<NfrSector> is_sector_override() const noexcept;

    bool operator<(const EmissionSector& other) const noexcept
    {
        return name() < other.name();
    }

    constexpr bool operator==(const EmissionSector& other) const noexcept
    {
        return _sector == other._sector;
    }

    constexpr bool operator!=(const EmissionSector& other) const noexcept
    {
        return !(*this == other);
    }

private:
    const NfrSector& get_nfr_sector() const noexcept;
    const GnfrSector& get_gnfr_sector() const noexcept;

    std::variant<NfrSector, GnfrSector> _sector;
};

class SectorInventory
{
public:
    SectorInventory(std::vector<GnfrSector> gnfrSectors, std::vector<NfrSector> nfrSectors, InputConversions gnfrSectorConversions, InputConversions nfrSectorConversions);

    EmissionSector sector_from_string(std::string_view name) const;
    EmissionSector sector_from_string(EmissionSector::Type type, std::string_view name) const;

    GnfrSector gnfr_sector_from_string(std::string_view str) const;
    NfrSector nfr_sector_from_string(std::string_view str) const;
    std::pair<NfrSector, int32_t> nfr_sector_with_priority_from_string(std::string_view str) const;

    GnfrSector gnfr_sector_from_id(GnfrId id) const;

    size_t gnfr_sector_count() const noexcept;
    size_t nfr_sector_count() const noexcept;

    bool is_ignored_sector(std::string_view str) const noexcept;

    std::span<const GnfrSector> gnfr_sectors() const noexcept;
    std::span<const NfrSector> nfr_sectors() const noexcept;

private:
    std::optional<GnfrSector> try_gnfr_sector_from_string(std::string_view str) const noexcept;
    std::optional<NfrSector> try_nfr_sector_from_string(std::string_view str) const noexcept;
    std::optional<std::pair<NfrSector, int32_t>> try_nfr_sector_with_priority_from_string(std::string_view str) const noexcept;

    // List of sectors used by the emap model
    std::vector<GnfrSector> _gnfrSectors;
    std::vector<NfrSector> _nfrSectors;

    // Mapping from all possible sector names in reported files to the emap names
    InputConversions _gnfrConversions;
    InputConversions _nfrConversions;
};

}

namespace fmt {
template <>
struct formatter<emap::EmissionSector>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const emap::EmissionSector& val, FormatContext& ctx)
    {
        return format_to(ctx.out(), val.name());
    }
};
}

namespace std {
// we want to use it with the std::unordered_* containers
template <>
struct hash<emap::GnfrId> : type_safe::hashable<emap::GnfrId>
{
};

template <>
struct hash<emap::NfrId> : type_safe::hashable<emap::NfrId>
{
};

}
