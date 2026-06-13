#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// CrashHandler
// 크래시 시그널(SIGSEGV/SIGABRT/SIGFPE/SIGBUS/SIGILL)과 미처리 C++ 예외
// (std::terminate)를 가로채, 죽기 직전에 백트레이스를 안전하게 stderr로 덤프한다.
//
// 안전성:
//   시그널 핸들러 안에서는 async-signal-safe 함수만 호출해야 한다
//   (malloc/printf/qInfo 등은 데드락·재크래시 위험). 따라서 write()와
//   backtrace_symbols_fd()만 사용한다. install()에서 backtrace()를 한 번
//   미리 호출해 libgcc lazy-load(malloc 유발)를 워밍업한다.
//
// 덤프 후에는 기본 핸들러로 복구해 재전달 → 코어덤프 생성 + 정상 종료 경로 유지.
// ─────────────────────────────────────────────────────────────────────────────
namespace CrashHandler {
    void install();
}
