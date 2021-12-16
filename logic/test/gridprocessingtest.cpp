#include "emap/gridprocessing.h"

#include "gdx/algo/sum.h"
#include "gdx/denseraster.h"
#include "gdx/denserasterio.h"

#include "infra/cliprogressbar.h"
#include "infra/hash.h"
#include "infra/test/containerasserts.h"
#include "infra/test/tempdir.h"
#include "testconfig.h"

#include <doctest/doctest.h>

namespace emap::test {

using namespace inf;
using namespace doctest;

static void sort_by_country(std::vector<CountryCellCoverage>& cov)
{
    std::sort(cov.begin(), cov.end(), [](const CountryCellCoverage& lhs, const CountryCellCoverage& rhs) {
        return lhs.first < rhs.first;
    });
}

//TEST_CASE("Grid processing" * may_fail())
//{
//    TempDir tmp("grid_country_extraction");
//
//    const auto inputPath     = fs::u8path(TEST_DATA_DIR) / "input" / "spatial_patterns" / "pm10_F_RoadTransport.tif";
//    const auto countriesPath = fs::u8path(EMAP_DATA_DIR) / "boundaries.gpkg";
//    const auto outputPath    = tmp.path();
//
//    /*SUBCASE("transform grid")
//    {
//        const auto inputPath   = fs::u8path("C:/Users/vdboerd/OneDrive - VITO/Documents/E-map/E-MAP/input/spatial patterns/co_A_PublicPower.tif");
//        const auto inputRaster = gdx::read_dense_raster<double>(inputPath);
//        auto result            = transform_grid(inputRaster, GridDefinition::Vlops1km);
//        gdx::write_raster(result, "c:/temp/trans.tif");
//    }*/
//
//    SUBCASE("extract countries [regression]")
//    {
//        ProgressBar progress(80);
//        const std::string spatialPatternFormat = "spatpat_{{}}.tif";
//
//        extract_countries_from_raster(
//            inputPath, GnfrSector::PublicPower, countriesPath, "Code3", outputPath, spatialPatternFormat, [&](const GridProcessingProgress::Status& status) {
//                progress.set_progress(status.progress());
//                progress.set_postfix_text(Country(status.payload()).full_name());
//                return ProgressStatusResult::Continue;
//            });
//
//        std::unordered_map<Country::Id, std::string> hashes = {
//            {Country::Id::AL, "fbfff3d45b90c8b38851f2fea40e679b"},
//            {Country::Id::AM, "fe12120a056b2be9c94fe8eb7d0ed2bf"},
//            {Country::Id::ARE, "0"},
//            {Country::Id::ARO, "0"},
//            {Country::Id::ASE, "0"},
//            {Country::Id::ASM, "0"},
//            {Country::Id::AT, "cc095798720282ca002a372cc98c1331"},
//            {Country::Id::ATL, "0"},
//            {Country::Id::AZ, "629fa23fc7adc2905c15a9c9779533fe"},
//            {Country::Id::BA, "9cab60240eb2c89a1535b9954d876422"},
//            {Country::Id::BAS, "0"},
//            {Country::Id::BEB, "0"},
//            {Country::Id::BEF, "0"},
//            {Country::Id::BEW, "0"},
//            {Country::Id::BG, "6a0b849b8421eef27d97783c86eb5528"},
//            {Country::Id::BLS, "0"},
//            {Country::Id::BY, "927eaf5a72264427d22f37765bad22ca"},
//            {Country::Id::CAS, "0"},
//            {Country::Id::CH, "4ee69f04dd502b9938c94cf0c36dd6c0"},
//            {Country::Id::CY, "dd6334c1b37b8056eecd523b4cde1f5a"},
//            {Country::Id::CZ, "dc9432cf1b756ce31c7848eef9c42e04"},
//            {Country::Id::DE, "51c7cb300c3d75af86b94e56fa180f7c"},
//            {Country::Id::DK, "d93a77a173c36baabeb9c65b3bda2612"},
//            {Country::Id::EE, "c6abdf843c3ba43134941d3860d3a8f2"},
//            {Country::Id::ES, "3b540c91489ae7c2e4bbbf56aa40ae80"},
//            {Country::Id::FI, "5456ad21764453d859262e3a68c53e97"},
//            {Country::Id::FR, "3fd1e621919a45860fbc359289ba1e26"},
//            {Country::Id::GB, "11d112a1eefb299e054d8533a17fb12f"},
//            {Country::Id::GE, "6f1ecbe0771d562e0c44f4fc2a671d9e"},
//            {Country::Id::GL, "0e0c718e3066962848f94bca6344782a"},
//            {Country::Id::GR, "a08713693ad6403e470076c2da151583"},
//            {Country::Id::HR, "50ed82df7ae0013aecc77c4bbfb370c9"},
//            {Country::Id::HU, "25dc2225ebb5b78ab070173bdd4fdae1"},
//            {Country::Id::IE, "29ac4264471b908c75e4ad618c7f09e8"},
//            {Country::Id::IS, "a2ced86586b0468f4c6a161dbdd9192f"},
//            {Country::Id::IT, "828a028ad2b2f7c44761a701371179bd"},
//            {Country::Id::KG, ""}, // outside of the spatial pattern map
//            {Country::Id::KZ, "f7aecc225403407ea25cb07e38ddc167"},
//            {Country::Id::KZE, "0"},
//            {Country::Id::LI, "e2602db80df76cd1f0f37bf7e795bef0"},
//            {Country::Id::LT, "8c1bd2a53c4d99473d249f0205af218e"},
//            {Country::Id::LU, "6c67fe2016fdd35aa01968a882ef95d1"},
//            {Country::Id::LV, "74b38f056fbc32516a74008d3f7611b5"},
//            {Country::Id::MC, "f894e73dd6b18aec6977e98f3d08ea45"},
//            {Country::Id::MD, "7eb0f8c79f62d93dad92cd72f7268870"},
//            {Country::Id::ME, "edf0cd43b631c88488920dc7fb915ac6"},
//            {Country::Id::MED, ""},
//            {Country::Id::MK, "d3ea754503b63e3760e97e48cc285ab5"},
//            {Country::Id::MT, "682432a384f31d07ab7c534996847653"},
//            {Country::Id::NL, "fe47400733c532f50d48136ed392f4ed"},
//            {Country::Id::NO, "5748b5459639901b86ee006c2e8cc6dc"},
//            {Country::Id::NOA, ""},
//            {Country::Id::NOS, ""},
//            {Country::Id::PL, "f9688852becb2259b53c0c087454109b"},
//            {Country::Id::PT, "4ff3d1cd29a2d95c3cc78d74f85a9de8"},
//            {Country::Id::RFE, ""},
//            {Country::Id::RO, "7724319e28daf10ba214ccf4facb605d"},
//            {Country::Id::RS, "01d61b893480d3d831e5821e56cc922b"},
//            {Country::Id::RU, "8e1d55859fc8c25e752a19ea877b5a64"},
//            {Country::Id::RUX, ""},
//            {Country::Id::SE, "d162428fe667872d14e12f5ae447a5ad"},
//            {Country::Id::SI, "3f3d8f4be99e19124ab47b5d37509ec4"},
//            {Country::Id::SK, "710d8749cc27ff122923faee666bb713"},
//            {Country::Id::TJ, ""},
//            {Country::Id::TME, ""},
//            {Country::Id::TMO, ""},
//            {Country::Id::TR, "14f211039d6a584cb7bb9c6d73c6c3c1"},
//            {Country::Id::UA, "1e69a4b31ba54b3a91b74a5bdc50c831"},
//            {Country::Id::UZE, ""},
//            {Country::Id::UZO, ""},
//        };
//
//        for (auto& [countryId, hash] : hashes) {
//            Country country(countryId);
//            const auto filePath = outputPath / fmt::format(spatialPatternFormat, country.code());
//
//            if (hash.empty()) {
//                REQUIRE_MESSAGE(!fs::exists(filePath), filePath.u8string());
//            } else {
//                REQUIRE_MESSAGE(fs::is_regular_file(filePath), fmt::format("{} ({})", filePath, country.full_name()));
//
//                const auto ras = gdx::read_dense_raster<double>(filePath);
//                std::span<const uint8_t> dataSpan(reinterpret_cast<const uint8_t*>(ras.data()), ras.size() * sizeof(double));
//
//                CHECK_MESSAGE(hash::md5_string(dataSpan) == hash, fmt::format("Hash mismatch for {} ({})", country.full_name(), country.code()));
//            }
//        }
//    }
//}

}