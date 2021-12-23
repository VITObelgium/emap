#include "spatialpatterninventory.h"
#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;
using namespace date;

TEST_CASE("Spatial pattern selection test")
{
    SpatialPatternInventory inv;
    inv.scan_dir(2021_y, 2016_y, fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation");

    {
        // Available in 2016 at gnfr level: Industry
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::CO, EmissionSector(NfrSector::Nfr1A2b));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2016" / "CAMS_emissions_REG-APv5.1_2016_co_B_Industry.tif");
        CHECK(spSource.pollutant == Pollutant::Id::CO);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr1A2b));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2016 at gnfr level: Industry, cams version in filename is different
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::CO, EmissionSector(NfrSector::Nfr1A1a));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2016" / "CAMS_emissions_REG-APv5.3_2016_co_A_PublicPower.tif");
        CHECK(spSource.pollutant == Pollutant::Id::CO);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr1A1a));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2016 at nfr
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::PM2_5, EmissionSector(NfrSector::Nfr5C2));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2016" / "CAMS_emissions_REG-APv5.1_2016_pm2_5_5C2.tif");
        CHECK(spSource.pollutant == Pollutant::Id::PM2_5);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr5C2));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Nfr);
        CHECK(spSource.year == 2016_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2015 at gnfr (not in 2016)
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::NOx, EmissionSector(NfrSector::Nfr3Da1));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2015" / "CAMS_emissions_REG-APv5.1_2015_nox_L_AgriOther.tif");
        CHECK(spSource.pollutant == Pollutant::Id::NOx);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr3Da1));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2015_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2017 at gnfr (not in 2016 or 2015)
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::NOx, EmissionSector(NfrSector::Nfr2D3d));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2017" / "CAMS_emissions_REG-APv5.1_2017_nox_E_Solvents.tif");
        CHECK(spSource.pollutant == Pollutant::Id::NOx);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr2D3d));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2017_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2018 at gnfr (not in 2016 or 2015 or 2017 or 2014)
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::NOx, EmissionSector(NfrSector::Nfr1B2b));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2018" / "CAMS_emissions_REG-APv5.1_2018_nox_D_Fugitives.tif");
        CHECK(spSource.pollutant == Pollutant::Id::NOx);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr1B2b));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2018_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // Available in 2010 at gnfr (not in 2016 or 2015 or 2017 or 2014 or 2018 or ...)
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::NOx, EmissionSector(NfrSector::Nfr1A4bi));
        CHECK(spSource.path == fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "rest" / "reporting_2021" / "2010" / "CAMS_emissions_REG-APv5.1_2010_nox_I_OffRoad.tif");
        CHECK(spSource.pollutant == Pollutant::Id::NOx);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr1A4bi));
        CHECK(spSource.sectorLevel == EmissionSector::Type::Gnfr);
        CHECK(spSource.year == 2010_y);
        CHECK(spSource.type == SpatialPatternSource::Type::SpatialPattern);
    }

    {
        // No spatial mapping available: Use uniform spread
        const auto spSource = inv.get_spatial_pattern(Pollutant::Id::NMVOC, EmissionSector(NfrSector::Nfr1A1a));
        CHECK(spSource.path.empty());
        CHECK(spSource.pollutant == Pollutant::Id::NMVOC);
        CHECK(spSource.sector == EmissionSector(NfrSector::Nfr1A1a));
        CHECK(spSource.type == SpatialPatternSource::Type::UnfiformSpread);
    }
}

}
