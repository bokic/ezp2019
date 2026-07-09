#!/usr/bin/env bash

set -e

rm -rf build

CMAKE_ARGS=("-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic")

if [ -n "$VERSION" ]; then
    CMAKE_ARGS+=("-DGIT_VERSION=$VERSION" "-DGIT_TAG=$VERSION")
fi

cmake -B build "${CMAKE_ARGS[@]}"
cmake --build build --config Release

cp build/compile_commands.json .
