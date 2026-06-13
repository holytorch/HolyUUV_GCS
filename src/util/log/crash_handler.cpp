#include "crash_handler.h"

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <execinfo.h>
#include <exception>

// ─────────────────────────────────────────────────────────────────────────────
// 모든 출력은 async-signal-safe 경로만 사용한다 (write / backtrace_symbols_fd).
// Qt 로깅(qInfo 등)은 시그널 컨텍스트에서 절대 호출하지 않는다.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// write()는 async-signal-safe. strlen도 마찬가지.
void safeWrite(const char* s)
{
    if (!s) return;
    const ssize_t r = ::write(STDERR_FILENO, s, std::strlen(s));
    (void)r;   // 핸들러 안이라 에러 처리할 길이 없음 — 경고만 억제
}

// backtrace_symbols_fd는 malloc 없이 fd로 직접 출력 → async-signal-safe.
// (malloc을 쓰는 backtrace_symbols와 달리 시그널 핸들러에서 안전)
void dumpBacktrace()
{
    void* frames[64];
    const int n = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
}

void signalHandler(int sig)
{
    const char* name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV (segmentation fault)"; break;
        case SIGABRT: name = "SIGABRT (abort)";              break;
        case SIGFPE:  name = "SIGFPE (floating point)";      break;
        case SIGBUS:  name = "SIGBUS (bus error)";           break;
        case SIGILL:  name = "SIGILL (illegal instruction)"; break;
        default: break;
    }

    safeWrite("\n=== CRASH: ");
    safeWrite(name);
    safeWrite(" ===\n");
    dumpBacktrace();
    safeWrite("=== end of backtrace ===\n");

    // 기본 핸들러로 복구 후 재전달 → 코어덤프 생성 + 셸에 올바른 종료 코드 전달
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void terminateHandler()
{
    safeWrite("\n=== CRASH: unhandled C++ exception (std::terminate) ===\n");

    // terminate 컨텍스트는 시그널 핸들러가 아니므로 예외 메시지 추출은 가능.
    if (std::exception_ptr eptr = std::current_exception()) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            safeWrite("what(): ");
            safeWrite(e.what());
            safeWrite("\n");
        } catch (...) {
            safeWrite("(non-std exception)\n");
        }
    }

    dumpBacktrace();
    safeWrite("=== end of backtrace ===\n");

    // SIGABRT를 기본 동작으로 돌린 뒤 abort → 백트레이스 중복 덤프 방지
    ::signal(SIGABRT, SIG_DFL);
    std::abort();
}

} // namespace


void CrashHandler::install()
{
    // backtrace()가 첫 호출 때 libgcc를 lazy-load(malloc 유발)할 수 있다.
    // 시그널 컨텍스트에서 안전하도록 여기서 한 번 미리 호출해 워밍업.
    void* warmup[1];
    ::backtrace(warmup, 1);

    ::signal(SIGSEGV, signalHandler);
    ::signal(SIGABRT, signalHandler);
    ::signal(SIGFPE,  signalHandler);
    ::signal(SIGBUS,  signalHandler);
    ::signal(SIGILL,  signalHandler);

    std::set_terminate(terminateHandler);
}
