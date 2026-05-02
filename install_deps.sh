#!/bin/bash
set -e

echo "=== HolyUUV GCS 의존성 설치 ==="

sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qtbase5-dev \
    libqt5serialport5-dev \
    qtlocation5-dev \
    qtpositioning5-dev \
    qtdeclarative5-dev \
    libqt5quickwidgets5 \
    qml-module-qtlocation \
    qml-module-qtpositioning

echo "=== 완료 ==="
echo "다음 단계: ./setup_mavlink.sh"
