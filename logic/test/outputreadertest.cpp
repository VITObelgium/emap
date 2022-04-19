#include "emap/configurationparser.h"
#include "outputreaders.h"

#include "infra/test/tempdir.h"
#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

TEST_CASE("Output readers")
{
    const auto brnPath = fs::u8path(TEST_DATA_DIR) / "CO_OPS_2020.brn";
    auto entries       = read_brn_output(brnPath);
}
}