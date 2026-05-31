#!/bin/bash
set -e

echo "========================================"
echo "HolyUUV GCS - AppImage Packaging"
echo "========================================"
echo

# ── 바이너리 확인 ─────────────────────────────
if [ ! -f "build/HolyUUV_GCS" ]; then
    echo "[ERROR] build/HolyUUV_GCS not found."
    echo "        Run: cmake -B build && cmake --build build -j\$(nproc)"
    exit 1
fi

# ── linuxdeployqt 다운로드 ─────────────────────
if [ ! -f "linuxdeployqt" ]; then
    echo "[1/4] Downloading linuxdeployqt..."
    wget -q --show-progress \
        "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" \
        -O linuxdeployqt
    chmod +x linuxdeployqt
else
    echo "[1/4] linuxdeployqt already present"
fi

# ── AppDir 구성 ───────────────────────────────
echo "[2/4] Setting up AppDir..."
APPDIR="AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp build/HolyUUV_GCS "$APPDIR/usr/bin/"

cat > "$APPDIR/usr/share/applications/holyuuv_gcs.desktop" << EOF
[Desktop Entry]
Name=HolyUUV GCS
Exec=HolyUUV_GCS
Icon=holyuuv_gcs
Type=Application
Categories=Science;Robotics;
Comment=Ground Control Station for Autonomous Underwater Vehicles
EOF

cp assets/HolyUUV_GCS.png \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/holyuuv_gcs.png"

# ── linuxdeployqt 실행 ────────────────────────
echo "[3/4] Bundling Qt libraries..."
./linuxdeployqt \
    "$APPDIR/usr/share/applications/holyuuv_gcs.desktop" \
    -qmldir=resources/qml \
    -appimage \
    -no-translations

# ── 결과 파일 정리 ────────────────────────────
echo "[4/4] Renaming output..."
APPIMAGE=$(ls HolyUUV_GCS*.AppImage 2>/dev/null | head -1)
if [ -n "$APPIMAGE" ]; then
    mv "$APPIMAGE" "HolyUUV_GCS.AppImage"
    echo
    echo "========================================"
    echo "[OK] AppImage created: HolyUUV_GCS.AppImage"
    echo "     Run: ./HolyUUV_GCS.AppImage"
    echo "========================================"
else
    echo "[ERROR] AppImage not found after packaging"
    exit 1
fi
