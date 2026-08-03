find_package(lyra CONFIG REQUIRED)

add_executable(emapcli
    main.cpp
    ${PROJECT_SOURCE_DIR}/Changelog.md
    $<$<PLATFORM_ID:Windows>:${CMAKE_BINARY_DIR}/version.rc>
)

target_link_libraries(emapcli
PRIVATE
    emaplogic
    Infra::infra
    bfg::lyra
)

target_link_options(emapcli PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++ -static-libgcc>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<NOT:$<PLATFORM_ID:Windows>>>:-fuse-ld=gold>
)

if(EMAP_STRIP_BINARY)
    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "EMAP_STRIP_BINARY requires a strip tool")
    endif()

    add_custom_command(TARGET emapcli POST_BUILD
        COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:emapcli>"
        COMMENT "Stripping emapcli"
        VERBATIM
    )
endif()

install(TARGETS emapcli
    BUNDLE DESTINATION . COMPONENT Runtime
    RESOURCE DESTINATION data COMPONENT Runtime
)
