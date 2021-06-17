add_executable(emapcli
    main.cpp
)

target_link_libraries(emapcli
PRIVATE
    emaplogic
    Infra::infra
    $<$<PLATFORM_ID:Windows>:${CMAKE_BINARY_DIR}/version.rc>
)

install(TARGETS emapcli
    BUNDLE DESTINATION . COMPONENT Runtime
    RESOURCE DESTINATION data COMPONENT Runtime
)
