#!/bin/sh
# Builds Crispy Doom for the 3DS. Run inside the devkitpro/devkitarm image,
# from the repository root, after tools/3ds/build-deps.sh.
set -eu

# The 3DS toolchain only searches the devkitPro folders.
cp -r deps-3ds/include deps-3ds/lib "$DEVKITPRO/portlibs/3ds/"

cmake -S . -B build-3ds \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_SDL2_NET=OFF
cmake --build build-3ds -j"$(nproc)" "$@"
