#!/bin/bash
set -e

# scripts/ 폴더 안에 있어도 항상 프로젝트 루트에서 실행되도록 이동
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1

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

# ── linuxdeploy + Qt 플러그인 다운로드 ─────────
if [ ! -f "linuxdeploy" ]; then
    echo "[1/4] Downloading linuxdeploy..."
    wget -q --show-progress \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
        -O linuxdeploy
    chmod +x linuxdeploy
else
    echo "[1/4] linuxdeploy already present"
fi

if [ ! -f "linuxdeploy-plugin-qt" ]; then
    echo "[1/4] Downloading linuxdeploy-plugin-qt..."
    wget -q --show-progress \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
        -O linuxdeploy-plugin-qt
    chmod +x linuxdeploy-plugin-qt
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

# ── 플러그인 수동 포함 ─────────────────────────
# linuxdeploy-plugin-qt가 자동으로 안 넣는 플러그인 카테고리를 직접 복사한다.
#   geoservices    : QtLocation OSM/Voyager 맵 플러그인
#   renderers      : Qt3D OpenGL 렌더러 (없으면 3D 맵에서 "Unable to find renderer plugin for opengl" 충돌)
#   sceneparsers / geometryloaders : Qt3D 모델/지오메트리 로더 (있으면 함께)
QT_PLUGIN_DIR=$(qmake -query QT_INSTALL_PLUGINS)
for cat in geoservices renderers sceneparsers geometryloaders; do
    if [ -d "$QT_PLUGIN_DIR/$cat" ]; then
        mkdir -p "$APPDIR/usr/plugins/$cat"
        cp "$QT_PLUGIN_DIR/$cat/"*.so "$APPDIR/usr/plugins/$cat/" 2>/dev/null || true
    fi
done

# ── 강제 포함 라이브러리 ───────────────────────
# AppImage 관례상 "데스크톱엔 항상 있다"고 가정해 제외되는 라이브러리들.
# 최소 설치(컨테이너/서버) 환경엔 없어서 실행이 깨지므로 직접 찾아 넣는다.
# libEGL/libGL 등 그래픽 드라이버 계열은 번들 금지(호스트 것 사용) — 여기 넣지 말 것.
FORCE_LIBS=()
for soname in libharfbuzz.so.0 libICE.so.6 libSM.so.6; do
    p=$(ldconfig -p | grep -m1 "$soname" | awk '{print $NF}')
    [ -n "$p" ] && FORCE_LIBS+=( --library "$p" )
done

# ── linuxdeploy 실행 ──────────────────────────
echo "[3/4] Bundling Qt libraries..."
APPIMAGE_EXTRACT_AND_RUN=1 \
QMAKE=$(which qmake) \
QML_SOURCES_PATHS=resources/qml \
EXTRA_QT_PLUGINS="geoservices;renderers" \
./linuxdeploy \
    --appdir "$APPDIR" \
    --plugin qt \
    "${FORCE_LIBS[@]}" \
    --output appimage

# ── 결과 파일 정리 ────────────────────────────
echo "[4/4] Renaming output..."
# linuxdeploy가 만든 산출물 (예: HolyUUV_GCS-x86_64.AppImage). 최종 이름과 다르면 rename.
APPIMAGE=$(ls HolyUUV_GCS*.AppImage 2>/dev/null | grep -v "^HolyUUV_GCS.AppImage$" | head -1)
if [ -n "$APPIMAGE" ]; then
    mv "$APPIMAGE" "HolyUUV_GCS.AppImage"
fi

if [ -f "HolyUUV_GCS.AppImage" ]; then
    echo
    echo "========================================"
    echo "[OK] AppImage created: HolyUUV_GCS.AppImage"
    echo "     Run: ./HolyUUV_GCS.AppImage"
    echo "========================================"
else
    echo "[ERROR] AppImage not found after packaging"
    exit 1
fi
