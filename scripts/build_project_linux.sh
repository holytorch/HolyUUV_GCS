#!/bin/bash

# scripts/ 폴더 안에 있어도 항상 프로젝트 루트에서 실행되도록 이동
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1

echo "========================================"
echo "HolyUUV GCS - Build Script"
echo "========================================"
echo

if [ -z "$DISPLAY" ]; then
    echo "Warning: DISPLAY is not set. Setting to :0"
    export DISPLAY=:0
fi

# ================
#      Check MAVLink
# ================
if [ ! -f "3rdparty/mavlink/include/mavlink/v2.0/ardupilotmega/mavlink.h" ]; then
    echo "[WARN] MAVLink headers not found."
    echo "       Run: ./setup_mavlink.sh"
    echo
fi

# ================
#      Clear
# ================
echo "[1/3] Cleaning build directory..."
if [ -d "build" ]; then
    rm -rf build
    if [ $? -ne 0 ]; then
        echo "[ERROR] Failed to delete build directory"
        exit 1
    fi
    echo "[OK] Build directory cleaned"
else
    echo "[INFO] Build directory does not exist, skipping clean"
fi
echo

# ================
#      Configure
# ================
echo "[2/3] Configuring with CMake..."
cmake -B build -G "Unix Makefiles"
if [ $? -ne 0 ]; then
    echo
    echo "[ERROR] CMake configuration failed!"
    echo
    echo "Please check:"
    echo "  1. CMake is installed       (sudo apt install cmake)"
    echo "  2. Build tools              (sudo apt install build-essential)"
    echo "  3. Qt5 dev packages         (sudo apt install qtbase5-dev qt5-qmake)"
    echo "  4. Qt5 SerialPort           (sudo apt install libqt5serialport5-dev)"
    echo "  5. MAVLink headers          (./setup_mavlink.sh)"
    echo
    exit 1
fi
echo "[OK] Configuration successful"
echo

# ================
#      Build
# ================
echo "[3/3] Building project..."
cmake --build build -- -j$(nproc)
if [ $? -ne 0 ]; then
    echo
    echo "[ERROR] Build failed!"
    echo
    exit 1
fi
echo "[OK] Build successful"
echo

echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo
echo "Executable: build/HolyUUV_GCS"
echo
