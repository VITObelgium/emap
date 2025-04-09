include(FindPackageHandleStandardArgs)

find_path(FastCppCsvParser_INCLUDE_DIR
    NAMES csv.h
    PATH_SUFFIXES fast-cpp-csv-parser
    HINTS ${FastCppCsvParser_ROOT_DIR}/include ${FastCppCsvParser_INCLUDEDIR}
)

find_package_handle_standard_args(FastCppCsvParser
    FOUND_VAR FastCppCsvParser_FOUND
    REQUIRED_VARS FastCppCsvParser_INCLUDE_DIR
)

mark_as_advanced(FastCppCsvParser_ROOT_DIR FastCppCsvParser_INCLUDE_DIR)

if(FastCppCsvParser_FOUND AND NOT TARGET FastCppCsvParser::csv)
    add_library(FastCppCsvParser::csv INTERFACE IMPORTED)
    set_target_properties(FastCppCsvParser::csv PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FastCppCsvParser_INCLUDE_DIR}"
    )
endif()
