cmake_minimum_required(VERSION 3.15)
project(ezp2019 LANGUAGES C)

# --- Standard Settings ---
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to RelWithDebInfo if no build type is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
endif()

# --- Dependencies ---
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

find_package(Python3 REQUIRED)

# --- Compiler Flags ---
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=format-security -fstack-protector-strong)
endif()

# Symbol visibility settings
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# --- Chip Database Generation ---
set(CHIP_HEADER "${CMAKE_CURRENT_BINARY_DIR}/ezp2019_chips.h")
add_custom_command(
    OUTPUT "${CHIP_HEADER}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_gen_header.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/ezp2019.dat"
            -o "${CHIP_HEADER}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_gen_header.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/ezp2019.dat"
    COMMENT "Generating ezp2019_chips.h from ezp2019.dat"
    VERBATIM
)

# --- Library Target ---
add_library(ezp2019 SHARED
    ezp2019.c
    ezp2019.h
    "${CHIP_HEADER}"
)

set_target_properties(ezp2019 PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER ezp2019.h
)

target_include_directories(ezp2019 PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(ezp2019 PRIVATE PkgConfig::LIBUSB)

# --- CLI Target ---
add_executable(ezp2019_cli main.c)
set_target_properties(ezp2019_cli PROPERTIES OUTPUT_NAME "ezp2019")
target_link_libraries(ezp2019_cli PRIVATE ezp2019)

# --- Installation ---
install(TARGETS ezp2019_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS ezp2019
    EXPORT ezp2019Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
