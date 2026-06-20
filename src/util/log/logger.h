#pragma once
#include <QDebug>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// Qt 로깅 래퍼. init()에서:
//   - 타임스탬프 포함 포맷 설정 ([hh:mm:ss.zzz] [type] message)
//   - 콘솔(stderr) + 파일 동시 출력 핸들러 설치
//   - 로그 파일: ~/.local/share/HolyUUV_GCS/logs/gcs_<yyyy-MM-dd>.log
//   - 용량(파일당)·개수 로테이션
//   - 같은 fd를 CrashHandler에 넘겨 크래시 백트레이스도 이 파일에 함께 기록
// shutdown()에서 핸들러 복구 + 파일을 닫는다.
// ─────────────────────────────────────────────────────────────────────────────
namespace Logger {
    void init();
    void shutdown();
    QString logFilePath();   // 현재 열린 로그 파일 경로 (열기 실패 시 빈 문자열)
}

#define LOG_DEBUG(...)  qDebug(__VA_ARGS__)
#define LOG_INFO(...)   qInfo(__VA_ARGS__)
