cmake_minimum_required(VERSION 3.8)

project(maxfirmata)

set(MAXFIRMATA_SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(MAXFIRMATA_LIB_DIR "${PROJECT_SOURCE_DIR}/libs")
set(MAXFIRMATA_PACKAGE_DIR "${PROJECT_SOURCE_DIR}/Package/maxfirmata")

set(MAXFIRMATA_MAX_BASE_DIR "${MAXFIRMATA_LIB_DIR}/max-sdk-base")
set(MAXFIRMATA_C74_SUPPORT_DIR "${MAXFIRMATA_MAX_BASE_DIR}/c74support")

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

include("${MAXFIRMATA_MAX_BASE_DIR}/script/max-pretarget.cmake")

add_library(maxfirmata SHARED "${MAXFIRMATA_SRC_DIR}/maxfirmata.c")

target_include_directories(maxfirmata PRIVATE
                        "${MAXFIRMATA_C74_SUPPORT_DIR}"
                        "${MAXFIRMATA_C74_SUPPORT_DIR}/max-includes")

find_library(MAX_API MaxAPI PATHS "${MAXFIRMATA_C74_SUPPORT_DIR}/max-includes" "${MAXFIRMATA_C74_SUPPORT_DIR}/max-includes/x64")
if(NOT MAX_API)
    message(FATAL_ERROR "MaxAPI not found")
endif(NOT MAX_API)

target_link_libraries(maxfirmata PRIVATE ${MAX_API})

set_target_properties(maxfirmata PROPERTIES PREFIX "")
set_target_properties(maxfirmata PROPERTIES OUTPUT_NAME "maxfirmata")

if(APPLE)
    set_target_properties(maxfirmata PROPERTIES SUFFIX "")
    install(TARGETS maxfirmata DESTINATION "${MAXFIRMATA_PACKAGE_DIR}/externals/maxfirmata.mxo/")
endif()
if(WIN32)
    set_target_properties(maxfirmata PROPERTIES SUFFIX ".mxe64")
    install(TARGETS maxfirmata DESTINATION "${MAXFIRMATA_PACKAGE_DIR}/externals/")
endif()