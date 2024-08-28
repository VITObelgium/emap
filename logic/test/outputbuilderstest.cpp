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

static RunConfiguration create_config(const SectorInventory& sectorInv, const PollutantInventory& pollutantInv, const CountryInventory& countryInv, ModelGrid grid, const fs::path& outputDir, bool poinSourcesSeparate)
{
    RunConfiguration::Output outputConfig;
    outputConfig.path                 = outputDir;
    outputConfig.outputLevelName      = "NFR";
    outputConfig.separatePointSources = poinSourcesSeparate;

    return RunConfiguration(file::u8path(TEST_DATA_DIR) / "_input", {}, {}, grid, ValidationType::NoValidation, 2016_y, 2021_y, "", 100.0, {}, sectorInv, pollutantInv, countryInv, outputConfig);
}

TEST_CASE("Output builders")
{
    TempDir tempDir("Output builder");

    const auto parametersPath     = file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters";
    const auto countryInventory   = parse_countries(file::u8path(TEST_DATA_DIR) / "_input" / "05_model_parameters" / "id_nummers.xlsx");
    const auto sectorInventory    = parse_sectors(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);
    const auto pollutantInventory = parse_pollutants(parametersPath / "id_nummers.xlsx", parametersPath / "code_conversions.xlsx", parametersPath / "names_to_be_ignored.xlsx", countryInventory);

    SUBCASE("Vlops")
    {
        const auto cfg     = create_config(sectorInventory, pollutantInventory, countryInventory, ModelGrid::Vlops1km, tempDir.path(), true);
        auto outputBuilder = make_output_builder(cfg);

        outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), Point<double>(100000, 120000), 2.0, 1000);
        outputBuilder->flush_pollutant(pollutants::CO, IOutputBuilder::WriteMode::Create);
        outputBuilder->flush(IOutputBuilder::WriteMode::Create);
    }

    SUBCASE("Chimere separate points")
    {
        const auto cfg     = create_config(sectorInventory, pollutantInventory, countryInventory, ModelGrid::Chimere05deg, tempDir.path(), true);
        auto outputBuilder = make_output_builder(cfg);

        outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), Point<double>(5.0, 35.0), 2.0, 1000);
        outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::CO), Point<double>(5.0, 36.0), 3.0, 1000);
        outputBuilder->add_point_output_entry(EmissionEntry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), EmissionValue(4.0), Point<double>(5.0, 36.0)));

        outputBuilder->flush_pollutant(pollutants::CO, IOutputBuilder::WriteMode::Create);
        outputBuilder->flush(IOutputBuilder::WriteMode::Create);

        REQUIRE(fs::is_regular_file(tempDir.path() / "output_Chimere_05deg_CO_2016.dat"));
        REQUIRE(fs::is_regular_file(tempDir.path() / "output_Chimere_pointsources_2016_ps.dat"));
        REQUIRE(fs::is_regular_file(tempDir.path() / "output_Chimere_header.dat"));

        auto headerLines = file::read_lines(tempDir.path() / "output_Chimere_header.dat");
        REQUIRE(headerLines.size() == 2);
        CHECK(headerLines[0] == "country col row 1A1a 1A1b 1A1c 1A2a 1A2b 1A2c 1A2d 1A2e 1A2f 1A2gvii 1A2gviii 1A3ai(i) 1A3aii(i) 1A3bi 1A3bii 1A3biii 1A3biv 1A3bv 1A3bvi 1A3bvii 1A3bviii 1A3c 1A3di(i) 1A3di(ii) 1A3dii 1A3ei 1A3eii 1A4ai 1A4aii 1A4bi 1A4bii 1A4ci 1A4cii 1A4ciii 1A5a 1A5b 1B1a 1B1b 1B1c 1B2ai 1B2aiv 1B2av 1B2b 1B2c 1B2d 2A1 2A2 2A3 2A5a 2A5b 2A5c 2A6 2B1 2B10a 2B10b 2B2 2B3 2B5 2B6 2B7 2C1 2C2 2C3 2C4 2C5 2C6 2C7a 2C7b 2C7c 2C7d 2D3a 2D3b 2D3c 2D3d 2D3e 2D3f 2D3g 2D3h 2D3i 2G 2H1 2H2 2H3 2I 2J 2K 2L 3B1a 3B1b 3B2 3B3 3B4a 3B4d 3B4e 3B4f 3B4gi 3B4gii 3B4giii 3B4giv 3B4h 3Da1 3Da2a 3Da2b 3Da2c 3Da3 3Da4 3Db 3Dc 3Dd 3De 3Df 3F 3I 5A 5B1 5B2 5C1a 5C1bi 5C1bii 5C1biii 5C1biv 5C1bv 5C1bvi 5C2 5D1 5D2 5D3 5E 6A");
        CHECK(headerLines[1].empty());

        auto diffuseLines = file::read_lines(tempDir.path() / "output_Chimere_05deg_CO_2016.dat");
        REQUIRE(diffuseLines.size() == 3);
        CHECK(container_contains(diffuseLines, "   1   32    1  2.000e+03  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00"));
        CHECK(container_contains(diffuseLines, "   1   32    3  0.000e+00  0.000e+00  0.000e+00  3.000e+03  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00"));
        CHECK(diffuseLines[2].empty());

        auto pointLines = file::read_lines(tempDir.path() / "output_Chimere_pointsources_2016_ps.dat");
        REQUIRE(pointLines.size() == 3);
        CHECK(container_contains(pointLines, "PIG      Long       Lat Country snap      temp     Vel  Height    Diam        CO       NH3     NMVOC       NOx      PM10     PM2.5  PMcoarse       SOx       TSP        BC        Pb        Cd        Hg        As        Cr        Cu        Ni        Se        Zn PCDD-PCDF       BaP       BbF       BkF    Indeno      PAHs       HCB      PCBs"));
        CHECK(container_contains(pointLines, "  0    5.0000   36.0000       1 7001     0.000   0.000   0.000   0.000  4000.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000     0.000"));
        CHECK(pointLines[2].empty());
    }

    SUBCASE("Chimere merged points")
    {
        const auto cfg     = create_config(sectorInventory, pollutantInventory, countryInventory, ModelGrid::Chimere05deg, tempDir.path(), false);
        auto outputBuilder = make_output_builder(cfg);

        outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), Point<double>(5.0, 35.0), 2.0, 1000);
        outputBuilder->add_diffuse_output_entry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::CO), Point<double>(5.0, 36.0), 3.0, 1000);
        outputBuilder->add_point_output_entry(EmissionEntry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A1a), pollutants::CO), EmissionValue(4.0), Point<double>(5.0, 36.0)));
        outputBuilder->add_point_output_entry(EmissionEntry(EmissionIdentifier(countries::AL, EmissionSector(sectors::nfr::Nfr1A2a), pollutants::CO), EmissionValue(5.0), Point<double>(5.0, 36.0)));

        outputBuilder->flush_pollutant(pollutants::CO, IOutputBuilder::WriteMode::Create);
        outputBuilder->flush(IOutputBuilder::WriteMode::Create);

        REQUIRE(fs::is_regular_file(tempDir.path() / "output_Chimere_05deg_CO_2016.dat"));
        REQUIRE(fs::is_regular_file(tempDir.path() / "output_Chimere_header.dat"));
        REQUIRE(!fs::exists(tempDir.path() / "output_Chimere_pointsources_2016_ps.dat"));

        auto diffuseLines = file::read_lines(tempDir.path() / "output_Chimere_05deg_CO_2016.dat");
        REQUIRE(diffuseLines.size() == 3);
        CHECK(container_contains(diffuseLines, "   1   32    1  2.000e+03  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00"));
        CHECK(container_contains(diffuseLines, "   1   32    3  4.000e+03  0.000e+00  0.000e+00  8.000e+03  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00  0.000e+00"));
        //                                        ^^^^^^^^^ Point with 4.0 added   ^^^^^^^^^ 3.0 + 5.0
        CHECK(diffuseLines[2].empty());
    }
}
}
