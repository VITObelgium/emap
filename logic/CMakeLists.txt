#find_package(TBB CONFIG REQUIRED)
#find_package(ParallelSTL CONFIG REQUIRED)
#find_package(Typesafe REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

add_library(emaplogic
    include/emap/emissions.h emissions.cpp
    include/emap/inputparsers.h inputparsers.cpp
    include/emap/scalingfactors.h scalingfactors.cpp
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
    #geodynamix::gdxcore
PRIVATE
    #TBB::tbb
    #pstl::ParallelSTL
    #Typesafe::Typesafe
    tomlplusplus::tomlplusplus
)

if (BUILD_TESTING)
    add_subdirectory(test)
endif ()