#include "crash_handler.h"

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <execinfo.h>
#include <exception>

// ─────────────────────────────────────────────────────────────────────────────
// All output goes through async-signal-safe paths only (write /
// backtrace_symbols_fd). Qt logging (qInfo, etc.) is never called from a signal
// context.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Log-file fd, set by Logger::init() via setLogFd(). A negative value skips the
// file dump. Read from a signal context, hence volatile sig_atomic_t.
volatile sig_atomic_t g_logFd = -1;

// write() and strlen() are both async-signal-safe.
// Writes to stderr and, if configured, to the log-file fd as well.
void safeWrite(const char* s)
{
    if (!s) return;
    const std::size_t len = std::strlen(s);
    const ssize_t r1 = ::write(STDERR_FILENO, s, len); (void)r1;
    if (g_logFd >= 0) { const ssize_t r2 = ::write(g_logFd, s, len); (void)r2; }
}

// backtrace_symbols_fd() writes straight to an fd without malloc, so it is
// async-signal-safe (unlike backtrace_symbols(), which allocates).
void dumpBacktrace()
{
    void* frames[64];
    const int n = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
    if (g_logFd >= 0) ::backtrace_symbols_fd(frames, n, g_logFd);
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

    // Restore the default handler and re-raise so a core dump is produced and the
    // shell receives the correct exit status.
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void terminateHandler()
{
    safeWrite("\n=== CRASH: unhandled C++ exception (std::terminate) ===\n");

    // A terminate context is not a signal handler, so the exception message can
    // be extracted here.
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

    // Restore SIGABRT's default action before aborting, to avoid dumping the
    // backtrace a second time.
    ::signal(SIGABRT, SIG_DFL);
    std::abort();
}

} // namespace


void CrashHandler::install()
{
    // backtrace()'s first call may lazy-load libgcc (triggering malloc). Warm it
    // up here once so the call is safe inside a signal context later.
    void* warmup[1];
    ::backtrace(warmup, 1);

    ::signal(SIGSEGV, signalHandler);
    ::signal(SIGABRT, signalHandler);
    ::signal(SIGFPE,  signalHandler);
    ::signal(SIGBUS,  signalHandler);
    ::signal(SIGILL,  signalHandler);

    std::set_terminate(terminateHandler);
}


void CrashHandler::setLogFd(int fd)
{
    g_logFd = fd;
}
