cmake_minimum_required(VERSION 3.5)

set (CMAKE_C_STANDARD 23)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

find_package(Python3 REQUIRED)

project(ezp2019 LANGUAGES C)

# Custom command to generate the chip database header
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_chips.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_gen_header.py 
            ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019.dat 
            -o ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_chips.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_gen_header.py 
            ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019.dat
    COMMENT "Generating ezp2019_chips.h from ezp2019.dat"
    VERBATIM
)

add_library(ezp2019 SHARED
    ezp2019.c
    ezp2019.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_chips.h
)
set_target_properties(ezp2019 PROPERTIES PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019.h)
target_include_directories(ezp2019 PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(ezp2019 PRIVATE ${LIBUSB_LIBRARIES})

add_executable(ezp2019_cli 
    main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ezp2019_chips.h
)
set_target_properties(ezp2019_cli PROPERTIES OUTPUT_NAME "ezp2019")
target_link_libraries(ezp2019_cli PRIVATE ezp2019)
