#!/usr/bin/env bash
# Builds everything the GamePad needs besides Cemu: drc-x264 + libdrc (patched), drc-hostap (patched), the bridge.
# Building in a container (distrobox, toolbox)? Run this whole script inside it.
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd)
cd "$root"
git -C .. submodule update --init gamepad-bridge/third_party/libdrc gamepad-bridge/third_party/drc-x264 \
  gamepad-bridge/third_party/drc-hostap-vanilla
./scripts/build-libdrc.sh
./scripts/build-drc-hostap.sh
cmake -S bridge -B build/bridge -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/bridge
echo
echo "Built. Next: ./scripts/install.sh  (installs the GamePad services; asks for your password once)"
