#include <QApplication>
#include <csignal>
#include <cstdio>
#include "application/Application.h"
#include "util/log/logger.h"
#include "util/log/crash_handler.h"
#include "info/version.h"

// ─────────────────────────────────────────────────────────────────────────────
// main()
// Initializes the Qt event loop and manages the Application lifecycle.
//
//   1. Initialize the logger and print version information
//   2. Application::initialize() — wire subsystems, detect links, show the window
//   3. Application::run()        — enter the Qt event loop (blocking)
//   4. Application::shutdown()   — teardown on exit
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // Install the crash handler first, so that any fault (SIGSEGV, etc.) or
    // unhandled exception raised during the stages below is captured with a
    // safe backtrace.
    CrashHandler::install();

    // Enable OpenGL context sharing so the 2D and 3D views draw from a single
    // shared resource pool rather than from isolated contexts.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Force the X11 (XCB) platform: under Wayland, Qt3DRender fails to create a
    // valid OpenGL context — a known issue with the Qt 5.15 + Wayland + Qt3D stack.
    qputenv("QT_QPA_PLATFORM", "xcb");

    // Follow the system GTK (dark) theme through the Qt5 platform theme.
    // (requires the qt5-gtk-platformtheme package)
    qputenv("QT_QPA_PLATFORMTHEME", "gtk3");

    // Initialize the Qt library (the QApplication instance is created here).
    QApplication qtApp(argc, argv);

    // Pin the application name explicitly. QStandardPaths (e.g. CacheLocation)
    // derives its path from applicationName; when unset it falls back to the
    // executable name, which under an AppImage becomes "AppRun.wrapped" and would
    // place the cache in ~/.cache/AppRun.wrapped/. Setting it here keeps the cache
    // at ~/.cache/HolyUUV_GCS/ regardless of how the app is launched.
    // (organizationName is intentionally left unset — setting it would nest the
    //  path as ~/.cache/<org>/HolyUUV_GCS/)
    QApplication::setApplicationName("HolyUUV_GCS");

    // On SIGINT (Ctrl+C) / SIGTERM (kill), quit the event loop gracefully so that
    // app.run() returns and app.shutdown() executes — the same exit path taken by
    // the window's close button.
    signal(SIGINT,  [](int) { QApplication::quit(); });
    signal(SIGTERM, [](int) { QApplication::quit(); });

    // Initialize the logger.
    Logger::init();

    // Startup banner — written directly to stderr so it bypasses the log pattern
    // ([time][type]); the subsequent [init] messages stream in just beneath it.
    std::fputs(
        "\n"
        "  ██╗  ██╗ ██████╗ ██╗     ██╗   ██╗██╗   ██╗██╗   ██╗██╗   ██╗\n"
        "  ██║  ██║██╔═══██╗██║     ╚██╗ ██╔╝██║   ██║██║   ██║██║   ██║\n"
        "  ███████║██║   ██║██║      ╚████╔╝ ██║   ██║██║   ██║██║   ██║\n"
        "  ██╔══██║██║   ██║██║       ╚██╔╝  ██║   ██║██║   ██║╚██╗ ██╔╝\n"
        "  ██║  ██║╚██████╔╝███████╗   ██║   ╚██████╔╝╚██████╔╝ ╚████╔╝ \n"
        "  ╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝    ╚═════╝  ╚═════╝   ╚═══╝  \n",
        stderr);
    std::fprintf(stderr,
        "\n          Ground Control Station   ·   v%d.%d.%d\n\n",
        HOLYUUV_GCS_VERSION_MAJOR,
        HOLYUUV_GCS_VERSION_MINOR,
        HOLYUUV_GCS_VERSION_PATCH);

    // Report where the log file is written, immediately below the banner.
    {
        const QString logPath = Logger::logFilePath();
        std::fprintf(stderr, "  Log file: %s\n\n",
                     logPath.isEmpty() ? "(failed to open — console output only)"
                                       : qUtf8Printable(logPath));
    }

    // Create and initialize the Application on the stack.
    Application app;

    if (!app.initialize()) {
        qCritical("Application initialization failed");
        Logger::shutdown();

        // A non-zero exit code signals abnormal termination.
        return -1;
    }

    const int exitCode = app.run();

    app.shutdown();
    Logger::shutdown();

    // Return the event-loop exit code (0 on a clean shutdown).
    return exitCode;
}
