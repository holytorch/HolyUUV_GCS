#!/bin/bash
set -e
# ─────────────────────────────────────────────────────────────────────────────
# Ubuntu 22.04 컨테이너에서 AppImage 를 빌드한다 → 22.04·24.04 모두에서 실행 가능.
# (오래된 glibc 기준으로 빌드 → 상위호환. QGC 의 "옛 배포판에서 빌드" 전략.)
#
# 호스트의 build/ 디렉터리는 건드리지 않는다 — 소스만 컨테이너 /work 로 복사해 빌드하고,
# 결과 AppImage 만 프로젝트 루트로 가져온다.
#
#   사용:  ./scripts/build_appimage_docker.sh
# ─────────────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE="holyuuv_build_2204"

echo "[1/2] 22.04 빌드 이미지 준비 (최초 1회 ~2-3분)..."
docker build -t "$IMAGE" -f "$PROJECT_DIR/test/Dockerfile.build.2204" "$PROJECT_DIR/test"

echo "[2/2] 컨테이너에서 빌드 + AppImage 패키징..."
docker run --rm \
    -v "$PROJECT_DIR":/project \
    "$IMAGE" \
    bash -lc '
        set -e
        mkdir -p /work && cd /work
        # 소스만 복사 (build/.git/AppDir/AppImage/_site 제외 → 호스트 산출물과 격리).
        # linuxdeploy* 는 그대로 복사돼 재다운로드를 아낀다.
        tar -C /project --exclude=./build --exclude=./.git --exclude=./AppDir \
            --exclude="./*.AppImage" --exclude=./docs/_site -cf - . | tar -xf -
        cmake -B build
        cmake --build build -j"$(nproc)"
        ./scripts/package_appimage.sh
        cp -f HolyUUV_GCS.AppImage /project/HolyUUV_GCS.AppImage
        # 컨테이너(root)가 만든 파일을 호스트 프로젝트 소유자(yoo)로 돌려준다.
        chown "$(stat -c "%u:%g" /project)" /project/HolyUUV_GCS.AppImage
        echo "[OK] /project/HolyUUV_GCS.AppImage 갱신 (22.04 빌드)"
    '

echo
echo "========================================"
echo "[완료] $PROJECT_DIR/HolyUUV_GCS.AppImage"
echo "  → 22.04·24.04 모두에서 실행 가능 (오래된 glibc 기준 빌드)"
echo "  검증: ./test/run_appimage_docker.sh 22.04  /  24.04"
echo "========================================"
