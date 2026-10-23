#!/bin/bash
# build-from-source.sh: builds the black-linux .deb yourself, on Linux Mint 22 / Ubuntu 24.04 (x86_64).
# No game files are needed for this step: it builds only the recompiler and the host libraries.
#   ./build-from-source.sh        then:  sudo apt install ./release/black-linux_*.deb
set -euo pipefail
cd "$(dirname "$0")"
sudo apt-get update
sudo apt-get install -y --no-install-recommends clang lld cmake ninja-build git ca-certificates pkg-config python3 \
    libvulkan-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libpulse-dev libasound2-dev libpipewire-0.3-dev \
    libudev-dev libdbus-1-dev libdrm-dev libgbm-dev libegl-dev dpkg-dev fakeroot
BUILD="${BUILD:-$HOME/.cache/black-linux-host-build}"
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --target stage -j "${JOBS:-2}"
./build-deb.sh "$BUILD" "$PWD/release"
