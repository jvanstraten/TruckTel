#!/bin/bash

GAME="/data/installs/steam/steamapps/common/Euro Truck Simulator 2/bin/linux_x64/eurotrucks2"
BUILD="build"
LIB="libtrucktel.so"
LOG="trucktel.txt"

PLUGINDIR="$(dirname "$GAME")/plugins"

set -xe

mkdir -p "$BUILD"
cmake -B "$BUILD"
cmake --build "$BUILD"
cp "$BUILD/$LIB" "$PLUGINDIR/$LIB"
truncate "$PLUGINDIR/$LOG" -s 0
"$GAME"
tail --follow "$PLUGINDIR/$LOG"
