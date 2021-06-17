add_executable(emapcli
    main.cpp
)

target_link_libraries(emapcli
PRIVATE
    #emaplogic
    Infra::infra
)

if (WIN32)
    target_sources(emapcli PRIVATE ${CMAKE_BINARY_DIR}/version.rc)
endif ()
