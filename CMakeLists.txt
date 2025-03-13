cmake_minimum_required(VERSION 3.5)

set (CMAKE_C_STANDARD 23)

project(ezp2019 LANGUAGES C)
add_library(ezp2019 SHARED
    ezp2019.c
    ezp2019.h
)
