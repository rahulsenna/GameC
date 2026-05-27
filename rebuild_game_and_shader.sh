#!/bin/bash
set -e

# Change to the directory of the script
cd "$(dirname "$0")"

mkdir -p build

echo "Compiling Shaders..."
xcrun -sdk macosx metal -c src/shaders.metal -o build/shaders.air
xcrun -sdk macosx metallib build/shaders.air -o build/shaders.metallib

echo "Compiling Game Code..."
clang++ -g -O0 \
    -std=c++14 -fno-exceptions -fno-rtti \
    -I libs/ \
    -dynamiclib \
    src/game.cpp src/math_utils.cpp libs/base_arena.cpp \
    -o build/game.dylib
