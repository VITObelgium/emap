#pragma once

#include <infra/span.h>

#include <fmt/core.h>
#include <optional>
#include <span>
#include <string_view>
#include <type_safe/strong_typedef.hpp>
#include <unordered_set>
#include <variant>

namespace emap {

class SectorInventory;

//enum class GnfrSector
//{
//    PublicPower,         // A
//    Industry,            // B
//    OtherStationaryComb, // C
//    Fugitive,            // D
//    Solvents,            // E
//    RoadTransport,       // F
//    Shipping,            // G
//    Aviation,            // H
//    Offroad,             // I
//    Waste,               // J
//    AgriLivestock,       // K
//    AgriOther,           // L
//    Other,               // M
//    EnumCount,
//};

//enum class NfrSector
//{
//    Nfr1A1a,
//    Nfr1A1b,
//    Nfr1A1c,
//    Nfr1A2a,
//    Nfr1A2b,
//    Nfr1A2c,
//    Nfr1A2d,
//    Nfr1A2e,
//    Nfr1A2f,
//    Nfr1A2gvii,
//    Nfr1A2gviii,
//    Nfr1A3ai_i,
//    Nfr1A3aii_i,
//    Nfr1A3bi,
//    Nfr1A3bii,
//    Nfr1A3biii,
//    Nfr1A3biv,
//    Nfr1A3bv,
//    Nfr1A3bvi,
//    Nfr1A3bvii,
//    Nfr1A3c,
//    Nfr1A3di_ii,
//    Nfr1A3dii,
//    Nfr1A3ei,
//    Nfr1A3eii,
//    Nfr1A4ai,
//    Nfr1A4aii,
//    Nfr1A4bi,
//    Nfr1A4bii,
//    Nfr1A4ci,
//    Nfr1A4cii,
//    Nfr1A4ciii,
//    Nfr1A5a,
//    Nfr1A5b,
//    Nfr1B1a,
//    Nfr1B1b,
//    Nfr1B1c,
//    Nfr1B2ai,
//    Nfr1B2aiv,
//    Nfr1B2av,
//    Nfr1B2b,
//    Nfr1B2c,
//    Nfr1B2d,
//    Nfr2A1,
//    Nfr2A2,
//    Nfr2A3,
//    Nfr2A5a,
//    Nfr2A5b,
//    Nfr2A5c,
//    Nfr2A6,
//    Nfr2B1,
//    Nfr2B2,
//    Nfr2B3,
//    Nfr2B5,
//    Nfr2B6,
//    Nfr2B7,
//    Nfr2B10a,
//    Nfr2B10b,
//    Nfr2C1,
//    Nfr2C2,
//    Nfr2C3,
//    Nfr2C4,
//    Nfr2C5,
//    Nfr2C6,
//    Nfr2C7a,
//    Nfr2C7b,
//    Nfr2C7c,
//    Nfr2C7d,
//    Nfr2D3a,
//    Nfr2D3b,
//    Nfr2D3c,
//    Nfr2D3d,
//    Nfr2D3e,
//    Nfr2D3f,
//    Nfr2D3g,
//    Nfr2D3h,
//    Nfr2D3i,
//    Nfr2G,
//    Nfr2H1,
//    Nfr2H2,
//    Nfr2H3,
//    Nfr2I,
//    Nfr2J,
//    Nfr2K,
//    Nfr2L,
//    Nfr3B1a,
//    Nfr3B1b,
//    Nfr3B2,
//    Nfr3B3,
//    Nfr3B4a,
//    Nfr3B4d,
//    Nfr3B4e,
//    Nfr3B4f,
//    Nfr3B4gi,
//    Nfr3B4gii,
//    Nfr3B4giii,
//    Nfr3B4giv,
//    Nfr3B4h,
//    Nfr3Da1,
//    Nfr3Da2a,
//    Nfr3Da2b,
//    Nfr3Da2c,
//    Nfr3Da3,
//    Nfr3Da4,
//    Nfr3Db,
//    Nfr3Dc,
//    Nfr3Dd,
//    Nfr3De,
//    Nfr3Df,
//    Nfr3F,
//    Nfr3I,
//    Nfr5A,
//    Nfr5B1,
//    Nfr5B2,
//    Nfr5C1a,
//    Nfr5C1bi,
//    Nfr5C1bii,
//    Nfr5C1biii,
//    Nfr5C1biv,
//    Nfr5C1bv,
//    Nfr5C1bvi,
//    Nfr5C2,
//    Nfr5D1,
//    Nfr5D2,
//    Nfr5D3,
//    Nfr5E,
//    Nfr6A,
//    Nfr1A3bi_fu,
//    Nfr1A3bii_fu,
//    Nfr1A3biii_fu,
//    Nfr1A3biv_fu,
//    Nfr1A3bv_fu,
//    Nfr1A3bvi_fu,
//    Nfr1A3bvii_fu,
//    EnumCount,
//    Invalid,
//};

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

enum class EmissionDestination
{
    Land,
    Sea,
    Invalid,
};

class GnfrSector
{
public:
    GnfrSector() noexcept
    {
    }

    GnfrSector(std::string_view name, GnfrId id, std::string_view description, EmissionDestination destination);

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
    std::string _name;
    EmissionDestination _destination = EmissionDestination::Invalid;
    std::string _description;
};

class NfrSector
{
public:
    NfrSector() noexcept
    {
    }

    NfrSector(std::string_view name, NfrId id, GnfrSector gnfr, std::string_view description);

    std::string_view name() const noexcept
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
    GnfrSector _gnfr;
    std::string _name;
    std::string _description;
    std::unordered_set<std::string> _inputNames;
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
    GnfrSector gnfr_sector() const noexcept;

    bool is_land_sector() const noexcept;
    /* Returns the nfr sector this sector overrides if it is applicable */
    //std::optional<NfrSector> is_sector_override() const noexcept;

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
    NfrSector get_nfr_sector() const noexcept;
    GnfrSector get_gnfr_sector() const noexcept;

    std::variant<NfrSector, GnfrSector> _sector;
};

class SectorInventory
{
public:
    SectorInventory(std::vector<GnfrSector> gnfrSectors, std::vector<NfrSector> nfrSectors);

    EmissionSector sector_from_string(std::string_view name) const;
    EmissionSector sector_from_string(EmissionSector::Type type, std::string_view name) const;

    GnfrSector gnfr_sector_from_string(std::string_view str) const;
    NfrSector nfr_sector_from_string(std::string_view str) const;

    GnfrSector gnfr_sector_from_id(GnfrId id) const;

    size_t gnfr_sector_count() const noexcept;
    size_t nfr_sector_count() const noexcept;

    std::span<const GnfrSector> gnfr_sectors() const noexcept;
    std::span<const NfrSector> nfr_sectors() const noexcept;

private:
    std::optional<GnfrSector> try_gnfr_sector_from_string(std::string_view str) const noexcept;
    std::optional<NfrSector> try_nfr_sector_from_string(std::string_view str) const noexcept;

    std::vector<GnfrSector> _gnfrSectors;
    std::vector<NfrSector> _nfrSectors;
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
