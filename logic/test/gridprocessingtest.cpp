#include "emap/gridprocessing.h"
#include "emap/configurationparser.h"
#include "geometry.h"
#include "testconstants.h"

#include "gdx/algo/sum.h"
#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"

#include "gdx/test/rasterasserts.h"

#include "infra/cliprogressbar.h"
#include "infra/hash.h"
#include "infra/log.h"
#include "infra/test/containerasserts.h"
#include "infra/test/tempdir.h"
#include "testconfig.h"

#include <doctest/doctest.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/GeometryFactory.h>

namespace emap::test {

using namespace inf;
using namespace doctest;
using namespace std::string_view_literals;

TEST_CASE("create_geometry_extent")
{
    auto geomFactory = geos::geom::GeometryFactory::create();

    const GeoMetadata gridMeta(4, 3, 50.0, -50.0, 50.0, {});
    geos::geom::CoordinateArraySequence coords(std::vector<geos::geom::Coordinate>({{110.0, 55.0}, {130.0, 75.0}, {120.0, 65.0}, {110.0, 55.0}}));

    const auto poly = geomFactory->createPolygon(geomFactory->createLinearRing(coords), {});

    auto meta = create_geometry_intersection_extent(*poly, gridMeta);
    CHECK(GeoMetadata(1, 1, 100.0, 50.0, 50.0, {}) == meta);
}

TEST_CASE("create_country_coverages")
{
    auto outputGrid    = grid_data(GridDefinition::Vlops60km).meta;
    auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

    CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
    auto vectorDs = gdal::warp_vector(countriesPath, grid_data(GridDefinition::Vlops60km).meta);

    CountryInventory countries({country::BEB});
    auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

    CHECK(coverageInfo.size() == 1);
    auto& beb = coverageInfo.front();

    for (const auto& cellInfo : beb.cells) {
        CHECK_MESSAGE(outputGrid.is_on_map(cellInfo.computeGridCell), "Not on spatial pattern extent: ", cellInfo.computeGridCell.r, " - ", cellInfo.computeGridCell.c);
        CHECK_MESSAGE(beb.outputSubgridExtent.is_on_map(cellInfo.countryGridCell), "Not on spatial pattern subgrid extent: ", cellInfo.countryGridCell.r, " - ", cellInfo.countryGridCell.c);
    }
}

TEST_CASE("create_country_coverages flanders chimere")
{
    auto outputGrid    = grid_data(GridDefinition::Chimere01deg).meta;
    auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

    CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
    auto vectorDs = gdal::warp_vector(countriesPath, grid_data(GridDefinition::Chimere01deg).meta);

    CountryInventory countries({country::BEF});
    auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

    CHECK(coverageInfo.size() == 1);
    auto& bef = coverageInfo.front();

    CHECK(bef.outputSubgridExtent.rows == 9);

    auto spatialPatternRaster = gdx::resample_raster(gdx::read_dense_raster<double>(fs::u8path(TEST_DATA_DIR) / "spatialpattern.tif"), bef.outputSubgridExtent, gdal::ResampleAlgorithm::Average);
    CHECK(spatialPatternRaster.metadata().rows == 9);

    const auto intersection = metadata_intersection(bef.outputSubgridExtent, outputGrid);
    CHECK(intersection.rows == 9);
}

TEST_CASE("create_country_coverages edge country")
{
    auto outputGrid    = grid_data(GridDefinition::Chimere01deg).meta;
    auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

    CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
    auto vectorDs = gdal::warp_vector(countriesPath, grid_data(GridDefinition::Chimere01deg).meta);

    CountryInventory countries({countries::NL});
    auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

    REQUIRE(coverageInfo.size() == 1);
    auto& nl = coverageInfo.front();

    const auto& countryExtent = nl.outputSubgridExtent;
    const auto intersection   = metadata_intersection(countryExtent, outputGrid);
    REQUIRE(intersection.bounding_box() == countryExtent.bounding_box());
}

TEST_CASE("Normalize raster")
{
    auto outputGrid    = grid_data(GridDefinition::Vlops60km).meta;
    auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

    CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
    auto vectorDs = gdal::warp_vector(countriesPath, grid_data(GridDefinition::Vlops60km).meta);

    CountryInventory countries({countries::NL});
    auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

    CHECK(coverageInfo.size() == 1);
    auto& nl = coverageInfo.front();

    auto grid = transform_grid(gdx::read_dense_raster<double>(fs::u8path(TEST_DATA_DIR) / "spatialpattern.tif"), GridDefinition::Vlops60km);

    auto nlRaster = extract_country_from_raster(sub_raster(grid, nl.outputSubgridExtent), nl);
    normalize_raster(nlRaster);
    CHECK(gdx::sum(nlRaster) == Approx(1.0));
}

TEST_CASE("Resample nodata check")
{
    auto gridDef = GridDefinition::Chimere005degLarge;

    auto outputGrid    = grid_data(gridDef).meta;
    auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

    CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
    auto vectorDs = gdal::warp_vector(countriesPath, grid_data(gridDef).meta);

    CountryInventory countries({countries::BEF});
    auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

    CHECK(coverageInfo.size() == 1);
    auto& bef = coverageInfo.front();

    auto inputPath = fs::u8path(TEST_DATA_DIR) / "1A3BI_NOX_2019.tif";
    auto result    = gdx::resample_raster(gdx::read_dense_raster<double>(inputPath), bef.outputSubgridExtent, gdal::ResampleAlgorithm::Average);

    size_t nodataCount = 0;
    for (auto cell : gdx::RasterCells(result)) {
        if (result.is_nodata(cell)) {
            ++nodataCount;
        }
    }

    // Less than 30% of the cells should have valid data
    CHECK((nodataCount / double(result.size())) < 0.3);
}

TEST_CASE("Add to raster")
{
    std::array<GridDefinition, 7> gridsToCheck = {
        GridDefinition::Chimere01deg,
        GridDefinition::Chimere05deg,
        GridDefinition::Chimere005degLarge,
        GridDefinition::Chimere0025deg,
        GridDefinition::ChimereRio4,
        GridDefinition::Vlops1km,
        GridDefinition::Vlops60km,
    };

    for (auto gridDef : gridsToCheck) {
        Log::info("Check grid: {}", grid_data(gridDef).name);

        auto outputGrid    = grid_data(gridDef).meta;
        auto countriesPath = fs::u8path(TEST_DATA_DIR) / "_input" / "03_spatial_disaggregation" / "boundaries" / "boundaries.gpkg";

        CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
        auto vectorDs = gdal::warp_vector(countriesPath, grid_data(gridDef).meta);

        CountryInventory countries({countries::BEF});
        auto coverageInfo = create_country_coverages(outputGrid, vectorDs, "Code3", countries, CoverageMode::GridCellsOnly, nullptr);

        CHECK(coverageInfo.size() == 1);
        auto& bef = coverageInfo.front();

        gdx::DenseRaster<double> grid1(outputGrid, 0.0);
        gdx::DenseRaster<double> grid2(outputGrid, 0.0);
        gdx::DenseRaster<double> flanders(bef.outputSubgridExtent, 0.0);

        for (auto& cellInfo : bef.cells) {
            flanders[cellInfo.countryGridCell] = 1.0;
            grid1[cellInfo.computeGridCell]    = 1.0;
        }

        add_to_raster(grid2, flanders);
        CHECK_RASTER_EQ(grid1, grid2);
    }
}

// TEST_CASE("res")
//{
//     auto spatialPattern = gdx::read_dense_raster<double>(fs::u8path(TEST_DATA_DIR) / "spatialpattern.tif");
//     auto resultCalc     = transform_grid(spatialPattern, GridDefinition::VlopsCalc, gdal::ResampleAlgorithm::Average);
//
//     auto result60Km = transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Average);
//     // auto result5Km  = transform_grid(resultCalc, GridDefinition::Vlops5km);
//
//     /*gdx::write_raster(resultCalc, "c:/temp/vlops_calc.tif");
//     gdx::write_raster(result60Km, "c:/temp/vlops_60km.tif");*/
//
//     auto sumCalc = resultCalc.sum();
//     auto sum60Km = result60Km.sum();
//
//     CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Average).sum(), "Average");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Bilinear).sum(), "Bilinear");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Cubic).sum(), "Cubic");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::CubicSpline).sum(), "CubicSpline");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Lanczos).sum(), "Lanczos");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Mode).sum(), "Mode");
//     // CHECK_MESSAGE((sumCalc / (240.0 * 240)) == transform_grid(resultCalc, GridDefinition::Vlops60km, gdal::ResampleAlgorithm::Sum).sum(), "Sum");
// }

// TEST_CASE("Grid processing" * may_fail())
//{
//     TempDir tmp("grid_country_extraction");
//
//     const auto inputPath     = fs::u8path(TEST_DATA_DIR) / "input" / "spatial_patterns" / "pm10_F_RoadTransport.tif";
//     const auto countriesPath = fs::u8path(EMAP_DATA_DIR) / "boundaries.gpkg";
//     const auto outputPath    = tmp.path();
//
//     /*SUBCASE("transform grid")
//     {
//         const auto inputPath   = fs::u8path("C:/Users/vdboerd/OneDrive - VITO/Documents/E-map/E-MAP/input/spatial patterns/co_A_PublicPower.tif");
//         const auto inputRaster = gdx::read_dense_raster<double>(inputPath);
//         auto result            = transform_grid(inputRaster, GridDefinition::Vlops1km);
//         gdx::write_raster(result, "c:/temp/trans.tif");
//     }*/
//
//     SUBCASE("extract countries [regression]")
//     {
//         ProgressBar progress(80);
//         const std::string spatialPatternFormat = "spatpat_{{}}.tif";
//
//         extract_countries_from_raster(
//             inputPath, GnfrSector::PublicPower, countriesPath, "Code3", outputPath, spatialPatternFormat, [&](const GridProcessingProgress::Status& status) {
//                 progress.set_progress(status.progress());
//                 progress.set_postfix_text(Country(status.payload()).full_name());
//                 return ProgressStatusResult::Continue;
//             });
//
//         std::unordered_map<Country::Id, std::string> hashes = {
//             {Country::Id::AL, "fbfff3d45b90c8b38851f2fea40e679b"},
//             {Country::Id::AM, "fe12120a056b2be9c94fe8eb7d0ed2bf"},
//             {Country::Id::ARE, "0"},
//             {Country::Id::ARO, "0"},
//             {Country::Id::ASE, "0"},
//             {Country::Id::ASM, "0"},
//             {Country::Id::AT, "cc095798720282ca002a372cc98c1331"},
//             {Country::Id::ATL, "0"},
//             {Country::Id::AZ, "629fa23fc7adc2905c15a9c9779533fe"},
//             {Country::Id::BA, "9cab60240eb2c89a1535b9954d876422"},
//             {Country::Id::BAS, "0"},
//             {Country::Id::BEB, "0"},
//             {Country::Id::BEF, "0"},
//             {Country::Id::BEW, "0"},
//             {Country::Id::BG, "6a0b849b8421eef27d97783c86eb5528"},
//             {Country::Id::BLS, "0"},
//             {Country::Id::BY, "927eaf5a72264427d22f37765bad22ca"},
//             {Country::Id::CAS, "0"},
//             {Country::Id::CH, "4ee69f04dd502b9938c94cf0c36dd6c0"},
//             {Country::Id::CY, "dd6334c1b37b8056eecd523b4cde1f5a"},
//             {Country::Id::CZ, "dc9432cf1b756ce31c7848eef9c42e04"},
//             {Country::Id::DE, "51c7cb300c3d75af86b94e56fa180f7c"},
//             {Country::Id::DK, "d93a77a173c36baabeb9c65b3bda2612"},
//             {Country::Id::EE, "c6abdf843c3ba43134941d3860d3a8f2"},
//             {Country::Id::ES, "3b540c91489ae7c2e4bbbf56aa40ae80"},
//             {Country::Id::FI, "5456ad21764453d859262e3a68c53e97"},
//             {Country::Id::FR, "3fd1e621919a45860fbc359289ba1e26"},
//             {Country::Id::GB, "11d112a1eefb299e054d8533a17fb12f"},
//             {Country::Id::GE, "6f1ecbe0771d562e0c44f4fc2a671d9e"},
//             {Country::Id::GL, "0e0c718e3066962848f94bca6344782a"},
//             {Country::Id::GR, "a08713693ad6403e470076c2da151583"},
//             {Country::Id::HR, "50ed82df7ae0013aecc77c4bbfb370c9"},
//             {Country::Id::HU, "25dc2225ebb5b78ab070173bdd4fdae1"},
//             {Country::Id::IE, "29ac4264471b908c75e4ad618c7f09e8"},
//             {Country::Id::IS, "a2ced86586b0468f4c6a161dbdd9192f"},
//             {Country::Id::IT, "828a028ad2b2f7c44761a701371179bd"},
//             {Country::Id::KG, ""}, // outside of the spatial pattern map
//             {Country::Id::KZ, "f7aecc225403407ea25cb07e38ddc167"},
//             {Country::Id::KZE, "0"},
//             {Country::Id::LI, "e2602db80df76cd1f0f37bf7e795bef0"},
//             {Country::Id::LT, "8c1bd2a53c4d99473d249f0205af218e"},
//             {Country::Id::LU, "6c67fe2016fdd35aa01968a882ef95d1"},
//             {Country::Id::LV, "74b38f056fbc32516a74008d3f7611b5"},
//             {Country::Id::MC, "f894e73dd6b18aec6977e98f3d08ea45"},
//             {Country::Id::MD, "7eb0f8c79f62d93dad92cd72f7268870"},
//             {Country::Id::ME, "edf0cd43b631c88488920dc7fb915ac6"},
//             {Country::Id::MED, ""},
//             {Country::Id::MK, "d3ea754503b63e3760e97e48cc285ab5"},
//             {Country::Id::MT, "682432a384f31d07ab7c534996847653"},
//             {Country::Id::NL, "fe47400733c532f50d48136ed392f4ed"},
//             {Country::Id::NO, "5748b5459639901b86ee006c2e8cc6dc"},
//             {Country::Id::NOA, ""},
//             {Country::Id::NOS, ""},
//             {Country::Id::PL, "f9688852becb2259b53c0c087454109b"},
//             {Country::Id::PT, "4ff3d1cd29a2d95c3cc78d74f85a9de8"},
//             {Country::Id::RFE, ""},
//             {Country::Id::RO, "7724319e28daf10ba214ccf4facb605d"},
//             {Country::Id::RS, "01d61b893480d3d831e5821e56cc922b"},
//             {Country::Id::RU, "8e1d55859fc8c25e752a19ea877b5a64"},
//             {Country::Id::RUX, ""},
//             {Country::Id::SE, "d162428fe667872d14e12f5ae447a5ad"},
//             {Country::Id::SI, "3f3d8f4be99e19124ab47b5d37509ec4"},
//             {Country::Id::SK, "710d8749cc27ff122923faee666bb713"},
//             {Country::Id::TJ, ""},
//             {Country::Id::TME, ""},
//             {Country::Id::TMO, ""},
//             {Country::Id::TR, "14f211039d6a584cb7bb9c6d73c6c3c1"},
//             {Country::Id::UA, "1e69a4b31ba54b3a91b74a5bdc50c831"},
//             {Country::Id::UZE, ""},
//             {Country::Id::UZO, ""},
//         };
//
//         for (auto& [countryId, hash] : hashes) {
//             Country country(countryId);
//             const auto filePath = outputPath / fmt::format(spatialPatternFormat, country.code());
//
//             if (hash.empty()) {
//                 REQUIRE_MESSAGE(!fs::exists(filePath), filePath.u8string());
//             } else {
//                 REQUIRE_MESSAGE(fs::is_regular_file(filePath), fmt::format("{} ({})", filePath, country.full_name()));
//
//                 const auto ras = gdx::read_dense_raster<double>(filePath);
//                 std::span<const uint8_t> dataSpan(reinterpret_cast<const uint8_t*>(ras.data()), ras.size() * sizeof(double));
//
//                 CHECK_MESSAGE(hash::md5_string(dataSpan) == hash, fmt::format("Hash mismatch for {} ({})", country.full_name(), country.code()));
//             }
//         }
//     }
// }
}