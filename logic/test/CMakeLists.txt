add_executable(emaplogictest
    testconfig.h.in
    testconstants.h
    testprinters.h
    emissioninventorytest.cpp
    gridprocessingtest.cpp
    gridtest.cpp
    inputparsertest.cpp
    outputbuilderstest.cpp
    spatialpatterninventorytest.cpp
    runconfigurationparsertest.cpp
)

target_compile_definitions(emaplogictest
PRIVATE EMAP_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
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
)

configure_file(testconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/testconfig.h)

add_test(NAME emaplogictest COMMAND emaplogictest)