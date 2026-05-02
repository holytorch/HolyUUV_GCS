#!/bin/bash

echo "========================================"
echo "MAVLink Headers Setup"
echo "========================================"
echo

MAVLINK_DIR="3rdparty/mavlink/include/mavlink/v2.0"

if [ -d "$MAVLINK_DIR" ]; then
    echo "[INFO] MAVLink headers already exist at $MAVLINK_DIR"
    echo "       Delete the directory and re-run to refresh."
    exit 0
fi

mkdir -p "3rdparty/mavlink/include/mavlink"

echo "Cloning c_library_v2..."
git clone --depth 1 https://github.com/mavlink/c_library_v2.git "$MAVLINK_DIR"

if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to clone MAVLink headers"
    echo "        Check network connection or clone manually:"
    echo "        git clone https://github.com/mavlink/c_library_v2.git $MAVLINK_DIR"
    exit 1
fi

echo
echo "[OK] MAVLink headers installed at: $MAVLINK_DIR"
echo "     ArduSub dialect: $MAVLINK_DIR/ardupilotmega/mavlink.h"
echo
