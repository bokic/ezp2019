cmake_minimum_required(VERSION 3.5)

set (CMAKE_C_STANDARD 23)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

project(ezp2019 LANGUAGES C)

add_library(ezp2019 SHARED
    ezp2019.c
    ezp2019.h
)
target_include_directories(ezp2019 PRIVATE ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(ezp2019 PRIVATE ${LIBUSB_LIBRARIES})

add_executable(ezp2019_cli main.c)
set_target_properties(ezp2019_cli PROPERTIES OUTPUT_NAME "ezp2019")
target_link_libraries(ezp2019_cli PRIVATE ezp2019)
