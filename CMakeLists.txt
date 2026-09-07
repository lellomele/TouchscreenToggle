cmake_minimum_required(VERSION 3.20)

project(TouchscreenToggle VERSION 1.0.0 LANGUAGES CXX RC)

set(APP_ICON_RES "${CMAKE_CURRENT_BINARY_DIR}/TouchscreenToggleIcon.res")
add_custom_command(
    OUTPUT "${APP_ICON_RES}"
    COMMAND "${CMAKE_RC_COMPILER}" /nologo
        "/fo${APP_ICON_RES}"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/touchscreen-toggle.ico"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Compiling the application icon"
    VERBATIM
)
set_source_files_properties("${APP_ICON_RES}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)

add_executable(TouchscreenToggle WIN32
    src/main.cpp
    "${APP_ICON_RES}"
)

target_compile_features(TouchscreenToggle PRIVATE cxx_std_17)
target_compile_definitions(TouchscreenToggle PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(TouchscreenToggle PRIVATE
    setupapi
    cfgmgr32
    comctl32
    dwmapi
)

if(MSVC)
    set_property(TARGET TouchscreenToggle APPEND PROPERTY
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
    target_compile_options(TouchscreenToggle PRIVATE /W4 /permissive- /utf-8)
    target_link_options(TouchscreenToggle PRIVATE
        /MANIFEST:EMBED
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
    set_property(TARGET TouchscreenToggle PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_custom_command(TARGET TouchscreenToggle POST_BUILD
        COMMAND "${CMAKE_MT}"
            -manifest "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest"
            "-outputresource:$<TARGET_FILE:TouchscreenToggle>;#1"
        COMMENT "Embedding the complete Windows application manifest"
        VERBATIM
    )
endif()

install(TARGETS TouchscreenToggle RUNTIME DESTINATION .)
