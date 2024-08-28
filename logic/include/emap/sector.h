#pragma once

#include "emap/emissiondestination.h"
#include "emap/inputconversion.h"
#include "infra/span.h"

#include <fmt/core.h>
#include <optional>
#include <span>
#include <string_view>
#include <type_safe/strong_typedef.hpp>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace emap {

class SectorInventory;

struct GnfrId : type_safe::strong_typedef<GnfrId, int32_t>,
                type_safe::strong_typedef_op::equality_comparison<GnfrId>,
                type_safe::strong_typedef_op::relational_comparison<GnfrId>
{
    using strong_typedef::strong_typedef;
};

struct NfrId : type_safe::strong_typedef<NfrId, int32_t>,
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
        return _destination == EmissionDestination::Land || _destination == EmissionDestination::Eez;
    }

    EmissionDestination destination() const noexcept
    {
        return _destination;
    }

    bool operator==(const GnfrSector& other) const noexcept
    {
        return id() == other.id();
    }

    bool operator!=(const GnfrSector& other) const noexcept
    {
        return !(*this == other);
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

    bool operator!=(const NfrSector& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(const NfrSector& other) const noexcept
    {
        return id() < other.id();
    }

    bool has_land_destination() const noexcept
    {
        return _destination == EmissionDestination::Land || _destination == EmissionDestination::Eez;
    }

    EmissionDestination destination() const noexcept
    {
        return _destination;
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

    /*! If it is a gnfr sector: throws RuntimeError
     *  If it is s nfr sector: returns the corresponding gnfr sector
     */
    const NfrSector& nfr_sector() const;

    bool is_valid() const noexcept;

    int32_t id() const noexcept;

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

}

template <>
struct fmt::formatter<emap::EmissionSector>
{
    FMT_CONSTEXPR20 auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        return ctx.begin();
    }

    auto format(const emap::EmissionSector& val, format_context& ctx) const -> format_context::iterator
    {
        return fmt::format_to(ctx.out(), "{}", val.name());
    }
};

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
