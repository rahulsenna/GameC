#!/bin/bash
set -e

cd "$(dirname "$0")"
mkdir -p build

build_shaders() {
    echo "Compiling Shaders..."
    xcrun -sdk macosx metal -c src/shaders.metal -o build/shaders.air
    xcrun -sdk macosx metallib build/shaders.air -o build/shaders.metallib.tmp
    mv build/shaders.metallib.tmp build/shaders.metallib
}

build_asset_builder() {
    echo "Compiling Asset Builder..."
    clang++ -g -O0 \
        -std=c++17 \
        -Wno-deprecated \
        -I include/ \
        src/asset_builder.cpp include/ufbx.c \
        -o build/asset_builder
}

build_game() {
    echo "Compiling Game Code..."
    UI_FLAG=""
    [ "$enable_ui" = true ] && UI_FLAG="-DENABLE_DEBUG_UI=1"
    clang++ -g -O0 \
        $UI_FLAG \
        -std=c++17 -fno-exceptions -fno-rtti \
        -Wno-deprecated \
        -I include/ \
        -dynamiclib \
        src/game.cpp src/animation.cpp src/math_utils.cpp include/base_arena.cpp src/shapes.cpp include/ufbx.c \
        lib/ozz/libozz_animation_r.a lib/ozz/libozz_base_r.a \
        -o build/game.dylib
}

build_engine() {
    echo "Compiling Engine Code..."
    UI_FLAG=""
    [ "$enable_ui" = true ] && UI_FLAG="-DENABLE_DEBUG_UI=1"
    clang++ -g -O0 \
        $UI_FLAG \
        -std=c++17 -fno-exceptions -fno-rtti \
        -I include/ \
        -framework Cocoa -framework Metal -framework QuartzCore \
        src/osx_main.mm src/renderer.cpp src/math_utils.cpp include/base_arena.cpp \
        -o build/engine
}

usage() {
    echo "Usage: $0 [-s] [-a] [-g] [-e] [-u] [-h]"
    echo "  -s   Build shaders only"
    echo "  -a   Build asset builder only"
    echo "  -g   Build game code only"
    echo "  -e   Build engine only"
    echo "  -u   Enable debug UI"
    echo "  -h   Show help"
}

do_shaders=false
do_asset_builder=false
do_game=false
do_engine=false
enable_ui=false
any_selected=false

while getopts ":sageuh" opt; do
    case "$opt" in
        s)
            do_shaders=true
            any_selected=true
            ;;
        a)
            do_asset_builder=true
            any_selected=true
            ;;
        g)
            do_game=true
            any_selected=true
            ;;
        e)
            do_engine=true
            any_selected=true
            ;;
        u)
            enable_ui=true
            ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG"
            usage
            exit 1
            ;;
    esac
done

if [ "$any_selected" = false ]; then
    do_shaders=true
    do_asset_builder=true
    do_game=true
    do_engine=true
fi

[ "$do_shaders" = true ] && build_shaders
[ "$do_asset_builder" = true ] && build_asset_builder
[ "$do_game" = true ] && build_game
[ "$do_engine" = true ] && build_engine

echo "Build Complete!"