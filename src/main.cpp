#include <QApplication>
#include <QLoggingCategory>
#include "application/Application.h"
#include "util/log/logger.h"
#include "info/version.h"

// ─────────────────────────────────────────────────────────────────────────────
// main()
// Qt 이벤트 루프를 초기화하고 Application의 수명 주기를 관리한다.
//
//   1. Logger 초기화 및 버전 정보 출력
//   2. Application::initialize() — 시스템 연결, 링크 감지, 창 표시
//   3. Application::run()        — Qt 이벤트 루프 진입 (블로킹)
//   4. Application::shutdown()   — 종료 정리
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    QLoggingCategory::setFilterRules("qt.location*=false");
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication qtApp(argc, argv);

    Logger::init();

    qInfo("=== %s V%d.%d.%d ===",
          HOLYUUV_GCS_NAME,
          HOLYUUV_GCS_VERSION_MAJOR,
          HOLYUUV_GCS_VERSION_MINOR,
          HOLYUUV_GCS_VERSION_PATCH);

    Application app;

    if (!app.initialize()) {
        qCritical("Application 초기화 실패");
        Logger::shutdown();
        return -1;
    }

    const int exitCode = app.run();

    app.shutdown();
    Logger::shutdown();

    return exitCode;
}
