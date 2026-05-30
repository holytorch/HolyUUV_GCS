#include <QApplication>
#include <csignal>
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

    // Wayland에서 Qt3DRender가 OpenGL 컨텍스트를 제대로 만들지 못하는 문제 회피.
    // X11(XCB)로 강제 — Qt5.15 + Wayland + Qt3D 조합 알려진 버그.
    qputenv("QT_QPA_PLATFORM", "xcb");

    // 시스템 GTK 다크 테마를 따라가도록 Qt5 플랫폼 테마 강제 지정
    // (qt5-gtk-platformtheme 패키지 필요)
    qputenv("QT_QPA_PLATFORMTHEME", "gtk3");

    // qt 라이브러리 초기화 (이후 QApplication 객체가 생성됨)
    QApplication qtApp(argc, argv);

    // Ctrl+C(SIGINT) / kill(SIGTERM) 수신 시 Qt 이벤트 루프를 정상 종료
    // → app.run() 반환 → app.shutdown() 호출로 X 버튼과 동일한 종료 경로 보장
    signal(SIGINT,  [](int) { QApplication::quit(); });
    signal(SIGTERM, [](int) { QApplication::quit(); });

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
