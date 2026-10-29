#include "emap/pollutant.h"

#include <compare>
#include <doctest/doctest.h>

namespace emap::test {

static_assert(std::three_way_comparable<Pollutant, std::strong_ordering>);

TEST_CASE("Pollutant comparison uses pollutant codes")
{
    const Pollutant lower("CO", "Z name");
    const Pollutant higher("NOx", "A name");
    const Pollutant sameCode("CO", "Different name");

    CHECK((lower <=> higher) == std::strong_ordering::less);
    CHECK((higher <=> lower) == std::strong_ordering::greater);
    CHECK((lower <=> lower) == std::strong_ordering::equal);
    CHECK((lower <=> sameCode) == std::strong_ordering::equal);
    CHECK(lower == sameCode);
    CHECK_FALSE(lower != sameCode);
    CHECK(lower != higher);
    CHECK(lower < higher);
    CHECK(higher > lower);
    CHECK(lower <= sameCode);
    CHECK(lower >= sameCode);

    const Pollutant empty;
    CHECK((empty <=> Pollutant("", "Different name")) == std::strong_ordering::equal);
    CHECK(empty < lower);
    CHECK(lower != Pollutant("co", "Lowercase code"));
    CHECK((lower <=> Pollutant("co", "Lowercase code")) == std::strong_ordering::less);
}

TEST_CASE("Pollutant supports defaulted containing comparisons")
{
    struct Entry
    {
        Pollutant pollutant;
        auto operator<=>(const Entry&) const = default;
    };

    static_assert(std::three_way_comparable<Entry, std::strong_ordering>);
    const Entry first{Pollutant("CO", "Carbon monoxide")};
    const Entry second{Pollutant("NOx", "Nitrogen oxides")};

    CHECK((first <=> second) == std::strong_ordering::less);
    CHECK(first < second);
    CHECK(first == first);
}

}
