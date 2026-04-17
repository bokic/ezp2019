#!/usr/bin/env bash

set -e

rm -rf build

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic"
cmake --build build --config Release

cp build/compile_commands.json .
