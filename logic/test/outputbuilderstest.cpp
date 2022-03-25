#include "emap/configurationparser.h"
#include "emap/outputbuilderfactory.h"

#include "infra/test/tempdir.h"
#include "testconfig.h"
#include "testconstants.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace date;
using namespace doctest;

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv, const fs::path& outputDir)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path            = outputDir;
    outputConfig.outputLevelName = "NFR";

    return RunConfiguration(fs::u8path(TEST_DATA_DIR) / "_input", {}, ModelGrid::Vlops1km, RunType::Emep, ValidationType::NoValidation, 2016_y, 2021_y, "", {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Output builders")
{
    TempDir tempDir("Output builder");

    const auto parametersPath     = fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx");
    const auto countryInventory   = parse_countries(fs::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx");

    const auto cfg = create_config(sectorInventory, pollutantInventory, countryInventory, tempDir.path());

    auto outputBuilder = make_output_builder(cfg);

    outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), Point<int64_t>(100000, 120000), 2.0, 1000);
    outputBuilder->flush_pollutant(pollutants::CO, IOutputBuilder::WriteMode::Create);
    outputBuilder->flush(IOutputBuilder::WriteMode::Create);
}
}