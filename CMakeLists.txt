cmake_minimum_required(VERSION 3.15)
# --- Version Detection ---
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(NOT GIT_VERSION)
    set(GIT_VERSION "0.0.0-unknown")
endif()

# Extract numeric part for CMake project() (e.g., 0.9.0 from 0.9.0-1-g...-dirty)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_NUMERIC_VERSION "${GIT_VERSION}")
if(NOT PROJECT_NUMERIC_VERSION)
    set(PROJECT_NUMERIC_VERSION "0.0.0")
endif()

project(ezp2019 
    VERSION ${PROJECT_NUMERIC_VERSION}
    LANGUAGES C
)

set(EZP2019_FULL_VERSION ${GIT_VERSION})

add_definitions(-DEZP2019_VERSION="${EZP2019_FULL_VERSION}")

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
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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
