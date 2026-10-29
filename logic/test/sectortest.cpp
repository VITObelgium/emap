#include "emap/sector.h"

#include <compare>
#include <doctest/doctest.h>
#include <limits>

namespace emap::test {

static_assert(std::three_way_comparable<GnfrSector, std::strong_ordering>);
static_assert(std::three_way_comparable<NfrSector, std::strong_ordering>);
static_assert(std::three_way_comparable<EmissionSector, std::strong_ordering>);

template <typename Sector>
static void check_sector_comparison(const Sector& lower, const Sector& higher, const Sector& sameId)
{
    CHECK((lower <=> higher) == std::strong_ordering::less);
    CHECK((higher <=> lower) == std::strong_ordering::greater);
    CHECK((lower <=> lower) == std::strong_ordering::equal);
    CHECK((lower <=> sameId) == std::strong_ordering::equal);
    CHECK(lower == sameId);
    CHECK_FALSE(lower != sameId);
    CHECK(lower != higher);
    CHECK(lower < higher);
    CHECK(higher > lower);
    CHECK(lower <= sameId);
    CHECK(lower >= sameId);
}

TEST_CASE("GNFR sector comparison uses sector IDs")
{
    const GnfrSector lower("ZZZ", GnfrId(std::numeric_limits<int32_t>::min()), "Z", "Lower ID", EmissionDestination::Land);
    const GnfrSector higher("AAA", GnfrId(std::numeric_limits<int32_t>::max()), "A", "Higher ID", EmissionDestination::Sea);
    const GnfrSector sameId("BBB", lower.id(), "B", "Different metadata", EmissionDestination::Eez);

    check_sector_comparison(lower, higher, sameId);
    check_sector_comparison(EmissionSector(lower), EmissionSector(higher), EmissionSector(sameId));
}

TEST_CASE("NFR sector comparison uses sector IDs")
{
    const GnfrSector firstParent("First", GnfrId(1), "A", "First parent", EmissionDestination::Land);
    const GnfrSector secondParent("Second", GnfrId(2), "B", "Second parent", EmissionDestination::Sea);
    const NfrSector lower("ZZZ", NfrId(std::numeric_limits<int32_t>::min()), firstParent, "Lower ID", EmissionDestination::Land);
    const NfrSector higher("AAA", NfrId(std::numeric_limits<int32_t>::max()), firstParent, "Higher ID", EmissionDestination::Sea);
    const NfrSector sameId("BBB", lower.id(), secondParent, "Different metadata and parent", EmissionDestination::Eez);

    check_sector_comparison(lower, higher, sameId);
    check_sector_comparison(EmissionSector(lower), EmissionSector(higher), EmissionSector(sameId));
}

TEST_CASE("Emission sector comparison distinguishes sector types")
{
    const GnfrSector gnfr("Same name", GnfrId(1), "A", "GNFR", EmissionDestination::Land);
    const NfrSector nfr("Same name", NfrId(1), gnfr, "NFR", EmissionDestination::Land);
    const EmissionSector nfrSector(nfr);
    const EmissionSector gnfrSector(gnfr);

    CHECK(nfrSector != gnfrSector);
    CHECK((nfrSector <=> gnfrSector) == std::strong_ordering::less);
    CHECK((gnfrSector <=> nfrSector) == std::strong_ordering::greater);
    CHECK(nfrSector < gnfrSector);
}

TEST_CASE("Sectors support defaulted containing comparisons")
{
    struct Entry
    {
        EmissionSector sector;
        auto operator<=>(const Entry&) const = default;
    };

    static_assert(std::three_way_comparable<Entry, std::strong_ordering>);
    const GnfrSector lower("ZZZ", GnfrId(1), "Z", "Lower ID", EmissionDestination::Land);
    const GnfrSector higher("AAA", GnfrId(2), "A", "Higher ID", EmissionDestination::Sea);
    const Entry first{EmissionSector(lower)};
    const Entry second{EmissionSector(higher)};

    CHECK((first <=> second) == std::strong_ordering::less);
    CHECK(first < second);
    CHECK(first == first);
}

}
