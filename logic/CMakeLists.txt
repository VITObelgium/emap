find_package(TBB CONFIG REQUIRED)
find_package(oneDPL CONFIG REQUIRED)
#find_package(Typesafe REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(polyclipping CONFIG REQUIRED)

add_library(emaplogic
    include/emap/country.h country.cpp
    include/emap/configurationparser.h configurationparser.cpp
    include/emap/emissions.h emissions.cpp
    include/emap/griddefinition.h griddefinition.cpp
    include/emap/gridprocessing.h gridprocessing.cpp
    include/emap/inputparsers.h inputparsers.cpp
    include/emap/modelrun.h modelrun.cpp
    include/emap/pollutant.h pollutant.cpp
    include/emap/preprocessing.h preprocessing.cpp
    include/emap/scalingfactors.h scalingfactors.cpp
    include/emap/sector.h sector.cpp
    include/emap/runconfiguration.h runconfiguration.cpp
    include/emap/preprocessingconfiguration.h preprocessingconfiguration.cpp
    enuminfo.h
    emissioninventory.h
    unitconversion.h
    geometry.h geometry.cpp
    ${CMAKE_SOURCE_DIR}/GSL.natvis
)

target_include_directories(emaplogic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_definitions(emaplogic PUBLIC __TBB_NO_IMPLICIT_LINKAGE PRIVATE NOMINMAX)

target_link_libraries(emaplogic
PUBLIC
    Infra::infra
    geodynamix::gdxcore
PRIVATE
    TBB::tbb
    oneDPL
    #Typesafe::Typesafe
    tomlplusplus::tomlplusplus
    polyclipping::polyclipping
)

if (BUILD_TESTING)
    add_subdirectory(test)
endif ()