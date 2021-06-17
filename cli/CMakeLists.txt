add_executable(emapcli
    main.cpp
    $<$<PLATFORM_ID:Windows>:${CMAKE_BINARY_DIR}/version.rc>
)

target_link_libraries(emapcli
PRIVATE
    emaplogic
    Infra::infra
)

install(TARGETS emapcli
    BUNDLE DESTINATION . COMPONENT Runtime
    RESOURCE DESTINATION data COMPONENT Runtime
)
