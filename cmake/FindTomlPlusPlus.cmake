include(FindPackageHandleStandardArgs)

find_path(TomlPlusPlus_INCLUDE_DIR
    NAMES toml++/toml.h
    HINTS ${TomlPlusPlus_ROOT_DIR}/include ${_VCPKG_INSTALLED_DIR}/include ${TomlPlusPlus_INCLUDEDIR}
)

find_library(TomlPlusPlus_LIBRARY NAMES tomlplusplus PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)
find_library(TomlPlusPlus_LIBRARY_DEBUG NAMES tomlplusplus PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)

message(STATUS "TomlPlusPlus library: Rel ${TomlPlusPlus_LIBRARY} Dbg ${TomlPlusPlus_LIBRARY_DEBUG}")

find_package_handle_standard_args(TomlPlusPlus
    FOUND_VAR TomlPlusPlus_FOUND
    REQUIRED_VARS TomlPlusPlus_INCLUDE_DIR TomlPlusPlus_LIBRARY
)

mark_as_advanced(
    TomlPlusPlus_ROOT_DIR
    TomlPlusPlus_INCLUDE_DIR
    TomlPlusPlus_LIBRARY
    TomlPlusPlus_LIBRARY_DEBUG
)

if(TomlPlusPlus_FOUND AND NOT TARGET TomlPlusPlus::TomlPlusPlus)
    add_library(TomlPlusPlus::TomlPlusPlus STATIC IMPORTED)
    set_target_properties(TomlPlusPlus::TomlPlusPlus PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        INTERFACE_INCLUDE_DIRECTORIES "${TomlPlusPlus_INCLUDE_DIR}"
        IMPORTED_LOCATION ${TomlPlusPlus_LIBRARY}
    )

    if(TomlPlusPlus_LIBRARY_DEBUG)
        set_target_properties(TomlPlusPlus::TomlPlusPlus PROPERTIES
            IMPORTED_LOCATION_DEBUG "${TomlPlusPlus_LIBRARY_DEBUG}"
        )
    endif()
endif()
