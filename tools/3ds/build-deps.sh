#!/bin/sh
# Builds SDL2 and SDL2_mixer for the 3DS into deps-3ds/ (devkitPro has no
# SDL2 packages for the 3DS; SDL supports it officially since 2.24).
# SDL2_mixer is built without external decoders: Doom's music is MIDI,
# played by Crispy Doom's own OPL emulation, and its sounds are raw PCM.
#
# Run inside the devkitpro/devkitarm image, from the repository root.
set -eu

SDL_VERSION=2.30.9
MIXER_VERSION=2.8.0
PREFIX="$PWD/deps-3ds"
WORK="$PWD/deps-3ds/src"

[ -f "$PREFIX/lib/libSDL2_mixer.a" ] && { echo "deps-3ds: already built"; exit 0; }

mkdir -p "$WORK"
[ -d "$WORK/SDL" ] || git clone -q --depth 1 --branch "release-$SDL_VERSION" \
    https://github.com/libsdl-org/SDL.git "$WORK/SDL"
[ -d "$WORK/SDL_mixer" ] || git clone -q --depth 1 --branch "release-$MIXER_VERSION" \
    https://github.com/libsdl-org/SDL_mixer.git "$WORK/SDL_mixer"

cmake -S "$WORK/SDL" -B "$WORK/SDL/build" \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
cmake --build "$WORK/SDL/build" -j"$(nproc)"
cmake --install "$WORK/SDL/build"

cmake -S "$WORK/SDL_mixer" -B "$WORK/SDL_mixer/build" \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DSDL2MIXER_SAMPLES=OFF \
    -DSDL2MIXER_CMD=OFF \
    -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_GME=OFF -DSDL2MIXER_MOD=OFF \
    -DSDL2MIXER_MP3=OFF -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_VORBIS=OFF \
    -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_WAVE=ON
cmake --build "$WORK/SDL_mixer/build" -j"$(nproc)"
cmake --install "$WORK/SDL_mixer/build"
