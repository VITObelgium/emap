cmake_minimum_required(VERSION 3.19)

project(e-map
    VERSION 3.1.1
    LANGUAGES CXX
)

enable_testing()

set(CMAKE_INSTALL_BINDIR ".")
set(CMAKE_INSTALL_DATAROOTDIR "data")
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)

option(INFRA_INSOURCE "Use submodule version of infra" ON)
option(GDX_INSOURCE "Use submodule version of geodynamix" ON)
set(PACKAGE_VERSION_COMMITHASH "dev" CACHE STRING "git commit hash")

if(PACKAGE_VERSION_COMMITHASH STREQUAL "dev")
    set(EMAP_DEV_BUILD 1)
else()
    set(EMAP_DEV_BUILD 0)

    if(CMAKE_COMPILER_IS_GNUCXX)
        add_link_options($<$<CONFIG:RELEASE>:-s>) # strip dist binaries
    endif()
endif()

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_VS_JUST_MY_CODE_DEBUGGING ON)

    # avoid windows specific warnings
    add_compile_definitions(
        _WIN32_WINNT=0x0601
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        _SILENCE_FPOS_SEEKPOS_DEPRECATION_WARNING
    )

    add_compile_options(
        $<$<CXX_COMPILER_ID:Clang>:-fdiagnostics-format=msvc>
        $<$<CXX_COMPILER_ID:MSVC>:/permissive->
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:twoPhase->
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
        $<$<CXX_COMPILER_ID:MSVC>:/w34100>
        $<$<CXX_COMPILER_ID:MSVC>:/w34101>
        $<$<CXX_COMPILER_ID:MSVC>:/w34265>
        $<$<CXX_COMPILER_ID:MSVC>:/w34287>
        $<$<CXX_COMPILER_ID:MSVC>:/w35038>
        $<$<CXX_COMPILER_ID:MSVC>:/await>
    )

    if(MSVC)
        # Fixes warning when linking against release c library in debug mode
        # Fixes warning when linking against release c library in debug mode
        add_link_options("$<$<CONFIG:Debug>:/NODEFAULTLIB:MSVCRT;/NODEFAULTLIB:LIBCMT>")
    endif()
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wfatal-errors -Wno-unknown-pragmas
        $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
        $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    )
endif()

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(INFRA_INSOURCE)
    set(INFRA_LOGGING ON)
    set(INFRA_GDAL ON)
    set(INFRA_PROJDB_DIR "data")
    set(INFRA_EMBED_GDAL_DATA OFF CACHE BOOL "" FORCE)
    set(INFRA_ENABLE_TESTS OFF)
    set(INFRA_CHARSET OFF)
    set(INFRA_HASHING ON)
    set(INFRA_CLI_PROGRESS ON)
    set(INFRA_ENABLE_TEST_UTILS ${BUILD_TESTING})
    set(INFRA_TESTUTIL_MAIN_PROJDB_PATH ${INFRA_PROJDB_DIR})

    add_subdirectory(deps/infra)
else()
    find_package(Infra CONFIG COMPONENTS testutil REQUIRED)
    set_target_properties(fmt::fmt PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set_target_properties(spdlog::spdlog PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set_target_properties(Infra::infra PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()

if(GDX_INSOURCE)
    set(GDX_PYTHON_BINDINGS OFF)
    set(GDX_ENABLE_TESTS OFF)
    set(GDX_ENABLE_TEST_UTILS ${BUILD_TESTING})
    set(GDX_ENABLE_TOOLS OFF)
    set(GDX_ENABLE_SIMD ON)

    IF(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64")
        set(GDX_AVX2 ON)
    endif()

    add_subdirectory(deps/geodynamix)
else()
    find_package(Geodynamix CONFIG REQUIRED)
endif()

add_subdirectory(logic)
add_subdirectory(cli)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/emapconfig.h)

if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
endif()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/Changelog.md
    DESTINATION .
)

add_subdirectory(dist)
