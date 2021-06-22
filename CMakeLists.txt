cmake_minimum_required(VERSION 3.15)

project(e-map
    VERSION 2.9.0
    LANGUAGES CXX
)

enable_testing()

set(CMAKE_INSTALL_BINDIR ".")
set(CMAKE_INSTALL_DATAROOTDIR "data")
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INFRA_INSOURCE "Use submodule version of infra" ON)
option(GDX_INSOURCE "Use submodule version of geodynamix" ON)
set(PACKAGE_VERSION_COMMITHASH "dev" CACHE STRING "git commit hash")
if (PACKAGE_VERSION_COMMITHASH STREQUAL "dev")
    set(EMAP_DEV_BUILD 1)
else ()
    set(EMAP_DEV_BUILD 0)
endif ()

if (WIN32)
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
    )

    if (MSVC)
        # Fixes warning when linking against release c library in debug mode
        # Fixes warning when linking against release c library in debug mode
        add_link_options("$<$<CONFIG:Debug>:/NODEFAULTLIB:MSVCRT;/NODEFAULTLIB:LIBCMT>")
        # Enable faster linking for debug builds
        add_link_options($<$<CONFIG:Debug>:/DEBUG:FASTLINK>)
    endif ()
else ()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wfatal-errors -Wno-unknown-pragmas
        $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
        $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    )
endif ()

set (LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if (INFRA_INSOURCE)
    set(INFRA_LOGGING ON CACHE BOOL "" FORCE)
    set(INFRA_GDAL ON CACHE BOOL "" FORCE)
    set(INFRA_EMBED_GDAL_DATA ON CACHE BOOL "" FORCE)
    set(INFRA_CHARSET ON CACHE BOOL "" FORCE)
    set(INFRA_CLI_PROGRESS ON CACHE BOOL "" FORCE)
    set(INFRA_ENABLE_TEST_UTILS ${BUILD_TESTING} CACHE BOOL "" FORCE)

    add_subdirectory(deps/infra)
else ()
    find_package(Infra CONFIG COMPONENTS testutil REQUIRED)
    set_target_properties(fmt::fmt PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set_target_properties(spdlog::spdlog PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set_target_properties(Infra::infra PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif ()

if (GDX_INSOURCE)
    set(GDX_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
    set(GDX_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(GDX_ENABLE_TEST_UTILS ON CACHE BOOL "" FORCE)
    set(GDX_ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory(deps/geodynamix)
else ()
    find_package(Geodynamix CONFIG REQUIRED)
endif ()

add_subdirectory(logic)
add_subdirectory(cli)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/legionellaconfig.h)
if (WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
endif ()

add_subdirectory(dist)
