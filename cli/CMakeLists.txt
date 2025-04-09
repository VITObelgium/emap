find_package(PROJ CONFIG REQUIRED)

add_executable(emapcli
    main.cpp
    ${PROJECT_SOURCE_DIR}/Changelog.md
    $<$<PLATFORM_ID:Windows>:${CMAKE_BINARY_DIR}/version.rc>
)

target_link_libraries(emapcli
PRIVATE
    emaplogic
    Infra::infra
)

set(RESOURCE_FILES
    $<$<CONFIG:Debug>:$<TARGET_FILE_DIR:PROJ::proj>/../../share/proj/proj.db>
    $<$<CONFIG:Release>:$<TARGET_FILE_DIR:PROJ::proj>/../share/proj/proj.db>
)

set_target_properties(emapcli PROPERTIES
    RESOURCE "${RESOURCE_FILES}"
)

install(TARGETS emapcli
    BUNDLE DESTINATION . COMPONENT Runtime
    RESOURCE DESTINATION data COMPONENT Runtime
)
