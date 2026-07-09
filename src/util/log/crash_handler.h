#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// CrashHandler
// Intercepts fatal signals (SIGSEGV / SIGABRT / SIGFPE / SIGBUS / SIGILL) and
// unhandled C++ exceptions (std::terminate), dumping a backtrace safely to
// stderr just before the process dies.
//
// Safety:
//   Only async-signal-safe functions may be called from within a signal handler
//   (malloc / printf / qInfo, etc. risk deadlock or a secondary crash). This
//   handler therefore uses only write() and backtrace_symbols_fd(). install()
//   invokes backtrace() once up front to warm up libgcc's lazy loading (which
//   would otherwise trigger malloc in the signal context).
//
// After dumping, the default handler is restored and the signal re-raised, so a
// core dump is produced and the normal termination path is preserved.
// ─────────────────────────────────────────────────────────────────────────────
namespace CrashHandler {
    void install();

    // Also dump the crash backtrace to this file descriptor, in addition to
    // stderr. Logger::init() opens the log file and passes its fd here (a
    // negative value skips the file dump). Only ::write / backtrace_symbols_fd
    // are used, so async-signal-safety is preserved.
    void setLogFd(int fd);
}
