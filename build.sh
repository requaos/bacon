#!/usr/bin/env bash
# Inject the Beacon module into upstream Meshtastic firmware and compile it.
#
# Usage: ./build.sh [platformio-env]
#   default env: rak_wismeshtag (RAK WisMesh Tag, nRF52840)
set -euo pipefail

TARGET_ENV=${1:-rak_wismeshtag}
FIRMWARE_DIR="firmware_build"

echo "1. Fetching upstream Meshtastic firmware..."
if [ ! -d "$FIRMWARE_DIR" ]; then
    git clone https://github.com/meshtastic/firmware.git "$FIRMWARE_DIR"
fi
cd "$FIRMWARE_DIR"
git pull
git submodule update --init --recursive

echo "2. Injecting Beacon Module..."
cp ../src/BeaconModule.* src/modules/

# setupModules() in src/modules/Modules.cpp creates every module.
# RoutingModule must stay the last module created, so insert just before it.
# NOTE: the guard must not match upstream's "modules/MeshBeaconModule.h".
if ! grep -q '"modules/BeaconModule.h"' src/modules/Modules.cpp; then
    sed -i 's|#include "modules/RoutingModule.h"|#include "modules/RoutingModule.h"\n#include "modules/BeaconModule.h"|' src/modules/Modules.cpp
    sed -i 's|routingModule = new RoutingModule();|beaconModule = new BeaconModule();\n    routingModule = new RoutingModule();|' src/modules/Modules.cpp
fi

echo "3. Compiling for $TARGET_ENV..."
pio run -e "$TARGET_ENV"

echo "Build complete! Binary located at: .pio/build/$TARGET_ENV/"
