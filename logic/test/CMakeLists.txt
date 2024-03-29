add_executable(emaplogictest
    testconfig.h.in
    testconstants.h
    testprinters.h
    emissioninventorytest.cpp
    emissioninventoryintegrationtest.cpp
    gridprocessingtest.cpp
    gridtest.cpp
    inputparsertest.cpp
    outputbuilderstest.cpp
    outputreadertest.cpp
    rasterbuildertest.cpp
    spatialpatterninventorytest.cpp
    runconfigurationparsertest.cpp
    emissioninventoryintegrationtest.cpp
)

target_compile_definitions(emaplogictest
PRIVATE EMAP_DATA_DIR="${PROJECT_SOURCE_DIR}/data" USE_UNSTABLE_GEOS_CPP_API
)
target_include_directories(emaplogictest
PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(emaplogictest
PRIVATE
    emaplogic
    Infra::infratestutilmain
    geodynamix::gdxtestutil
)

configure_file(testconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/testconfig.h)

add_test(NAME emaplogictest COMMAND emaplogictest)