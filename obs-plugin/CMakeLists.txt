cmake_minimum_required(VERSION 3.28)

project(soverlay-plugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT DEFINED OBS_SDK_DIR)
    set(OBS_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio-sdk")
endif()

set(OBS_BUILD_DIR "${OBS_SDK_DIR}/build")
set(OBS_LIB_CONFIG "RelWithDebInfo" CACHE STRING "Config subfolder used for the libobs build")

add_library(soverlay-plugin MODULE
    src/plugin-main.c
    src/show-onscreen-filter.c
    src/link-server.cpp
)

target_include_directories(soverlay-plugin PRIVATE
    "${OBS_SDK_DIR}/libobs"
    "${OBS_BUILD_DIR}/config"
)

target_link_libraries(soverlay-plugin PRIVATE
    "${OBS_BUILD_DIR}/libobs/${OBS_LIB_CONFIG}/obs.lib"
    ws2_32
)

target_compile_definitions(soverlay-plugin PRIVATE
    SOVERLAY_PLUGIN_VERSION="${PROJECT_VERSION}"
)

set_target_properties(soverlay-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "soverlay-plugin"
)

if(WIN32)
    target_compile_definitions(soverlay-plugin PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
endif()
