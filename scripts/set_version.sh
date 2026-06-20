#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# 버전 일괄 변경 — 단일 명령으로 모든 버전 표기를 맞춘다.
#   사용:  ./scripts/set_version.sh 1.2.0
#
# 단일 소스는 CMakeLists.txt 의 PROJECT(... VERSION x.y.z) 이고,
# C++ 의 src/info/version.h 는 빌드 때 거기서 자동 생성된다(version.h.in 템플릿).
# 이 스크립트는 빌드와 무관한 정적 파일(README 등)까지 같은 버전으로 함께 갱신한다.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

V="${1:-}"
if [[ ! "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "사용법: $0 X.Y.Z   (예: $0 1.2.0)" >&2
    exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 1) 단일 소스: CMakeLists.txt 의 PROJECT(... VERSION ...)  → C++ version.h 가 여기서 파생
sed -i -E "s/(PROJECT\(HolyUUV_GCS VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1${V}/" "$ROOT/CMakeLists.txt"

# 2) README 제목 (정적 파일이라 함께 갱신)
sed -i -E "s/(^# HolyUUV_GCS v)[0-9]+\.[0-9]+\.[0-9]+/\1${V}/" "$ROOT/README.md"

echo "버전을 ${V} 로 설정했습니다:"
echo "  - CMakeLists.txt   PROJECT(... VERSION ${V})   ← C++ version.h 의 단일 소스"
echo "  - README.md        # HolyUUV_GCS v${V}"
echo "다음 빌드 때 src/info/version.h 가 ${V} 로 자동 생성됩니다."
