#!/bin/bash
set -e

echo "========================================"
echo "HolyUUV GCS - Dependency Installation"
echo "========================================"
echo

sudo apt update

# ── 빌드 도구 ────────────────────────────────
sudo apt install -y \
    build-essential \
    cmake \
    git

# ── Qt5 개발 헤더 ────────────────────────────
sudo apt install -y \
    qtbase5-dev \
    qtbase5-private-dev \
    libqt5serialport5-dev \
    qtlocation5-dev \
    qtpositioning5-dev \
    qtdeclarative5-dev \
    libqt5quickwidgets5 \
    qt3d5-dev \
    libqt5sql5-sqlite

# ── QML 런타임 모듈 (실행 시 필요) ──────────
sudo apt install -y \
    qml-module-qtquick2 \
    qml-module-qtquick-controls2 \
    qml-module-qtquick-layouts \
    qml-module-qtquick-window2 \
    qml-module-qtlocation \
    qml-module-qtpositioning \
    qml-module-qtgraphicaleffects \
    qml-module-qt3d \
    qml-module-qtquick-scene3d

echo
echo "========================================"
echo "[OK] All Qt5 system dependencies installed"
echo "========================================"
echo

# ── MAVLink 헤더 (써드파티, header-only) ─────
# apt 패키지 없음 — git clone으로 설치
echo "[INFO] Installing MAVLink headers..."
if [ -f "./setup_mavlink.sh" ]; then
    bash ./setup_mavlink.sh
else
    echo "[WARN] setup_mavlink.sh not found — skipping MAVLink setup"
fi

echo
echo "========================================"
echo "All dependencies installed successfully!"
echo "========================================"
echo
echo "Next step:"
echo "  cmake -B build && cmake --build build -j\$(nproc)"
echo "  ./build/HolyUUV_GCS"
