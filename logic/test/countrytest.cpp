#include "emap/country.h"

#include <compare>
#include <doctest/doctest.h>
#include <limits>

namespace emap::test {

static_assert(std::three_way_comparable<Country, std::strong_ordering>);

TEST_CASE("Country comparison uses country IDs")
{
    const Country lower(CountryId(std::numeric_limits<int32_t>::min()), "ZZZ", "Lower ID", true);
    const Country higher(CountryId(std::numeric_limits<int32_t>::max()), "AAA", "Higher ID", false);
    const Country sameId(lower.id(), "BBB", "Different metadata", false);

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

TEST_CASE("Country supports defaulted containing comparisons")
{
    struct Entry
    {
        Country country;
        auto operator<=>(const Entry&) const = default;
    };

    static_assert(std::three_way_comparable<Entry, std::strong_ordering>);
    const Entry first{country::BEF};
    const Entry second{country::BEB};

    CHECK((first <=> second) == std::strong_ordering::less);
    CHECK(first < second);
    CHECK(first == first);
}

}
