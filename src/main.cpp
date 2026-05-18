#include <QApplication>
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
    // OpenGL 컨텍스트 공유 활성화 (2d, 3d 가 격리되지않고, 동일한 리소스 풀을 사용하도록)
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // qt 라이브러리 초기화 (이후 QApplication 객체가 생성됨)
    QApplication qtApp(argc, argv);

    // Logger 초기화
    Logger::init();

    qInfo("=== %s V%d.%d.%d ===",
          HOLYUUV_GCS_NAME,
          HOLYUUV_GCS_VERSION_MAJOR,
          HOLYUUV_GCS_VERSION_MINOR,
          HOLYUUV_GCS_VERSION_PATCH);

    // Application 스택 메모리 객체 생성 및 초기화
    Application app;

    if (!app.initialize()) {
        qCritical("Application 초기화 실패");
        Logger::shutdown();

        // -1은 비정상 종료라고 하자
        return -1;
    }

    const int exitCode = app.run();

    app.shutdown();
    Logger::shutdown();

    // int 0은 반환 : 정상 종료
    return exitCode;
}
