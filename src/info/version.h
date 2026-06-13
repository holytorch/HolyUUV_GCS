#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// 버전 상수
// CMake에서 HOLYUUV_GCS_BUILD_DATE를 빌드 날짜(YYYYMMDD)로 정의해 전달한다.
// ─────────────────────────────────────────────────────────────────────────────
#define HOLYUUV_GCS_NAME            "HolyUUV GCS"
#define HOLYUUV_GCS_MANUFACTURER    "HolyTorch"
#define HOLYUUV_GCS_VERSION_MAJOR   1
#define HOLYUUV_GCS_VERSION_MINOR   0
#define HOLYUUV_GCS_VERSION_PATCH   3

#ifndef HOLYUUV_GCS_BUILD_DATE
#define HOLYUUV_GCS_BUILD_DATE "00000000"
#endif
