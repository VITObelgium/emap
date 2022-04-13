find_package(TBB CONFIG REQUIRED)
find_package(oneDPL CONFIG REQUIRED)
find_package(GEOS CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(FastCppCsvParser REQUIRED)
find_package(type_safe CONFIG REQUIRED)
find_package(XlsxWriter REQUIRED)

add_library(emaplogic
    include/emap/constants.h
    include/emap/configurationparser.h configurationparser.cpp
    include/emap/country.h country.cpp
    include/emap/countryborders.h countryborders.cpp
    include/emap/debugtools.h debugtools.cpp
    include/emap/emissions.h emissions.cpp
    include/emap/emissiondestination.h
    include/emap/emissioninventory.h emissioninventory.cpp
    include/emap/griddefinition.h griddefinition.cpp
    include/emap/gridprocessing.h gridprocessing.cpp
    include/emap/inputconversion.h
    include/emap/inputparsers.h inputparsers.cpp
    include/emap/modelrun.h modelrun.cpp
    include/emap/modelpaths.h modelpaths.cpp
    include/emap/pollutant.h pollutant.cpp
    include/emap/scalingfactors.h scalingfactors.cpp
    include/emap/spatialpatterndata.h
    include/emap/sector.h sector.cpp
    include/emap/sectorinventory.h sectorinventory.cpp
    include/emap/sectorparameterconfig.h
    include/emap/runconfiguration.h runconfiguration.cpp
    include/emap/outputbuilderinterface.h
    include/emap/outputbuilderfactory.h outputbuilderfactory.cpp
    brnoutputentry.h
    brnanalyzer.h
    chimereoutputbuilder.h chimereoutputbuilder.cpp
    configurationutil.h
    datoutputentry.h
    enuminfo.h
    emissionvalidation.h emissionvalidation.cpp
    unitconversion.h
    geometry.h geometry.cpp
    gridrasterbuilder.h
    emissionscollector.h emissionscollector.cpp
    outputwriters.h outputwriters.cpp
    outputreaders.h outputreaders.cpp
    runsummary.h runsummary.cpp
    spatialpatterninventory.h spatialpatterninventory.cpp
    vlopsoutputbuilder.h vlopsoutputbuilder.cpp
    xlsxworkbook.h
    ${CMAKE_SOURCE_DIR}/GSL.natvis
    $<$<CXX_COMPILER_ID:MSVC>:${CMAKE_SOURCE_DIR}/emap.natvis>
)

target_include_directories(emaplogic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_definitions(emaplogic PUBLIC __TBB_NO_IMPLICIT_LINKAGE PRIVATE NOMINMAX USE_UNSTABLE_GEOS_CPP_API)

target_link_libraries(emaplogic
PUBLIC
    Infra::infra
    geodynamix::gdxcore
    type_safe
    GEOS::geos
PRIVATE
    TBB::tbb
    oneDPL
    FastCppCsvParser::csv
    tomlplusplus::tomlplusplus
    XlsxWriter::XlsxWriter
)

if (BUILD_TESTING)
    add_subdirectory(test)
endif ()