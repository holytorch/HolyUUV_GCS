#include <QApplication>
#include "application/Application.h"

int main(int argc, char* argv[])
{
    // qt 이벤트 루프 초기화(main이 끝나면 프로그램 종료하는데 그러지말라고 루프를 돌려야함)
    //app.exec()를 실행해야 루프가 시작됨
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);

    qInfo("=== HolyUUV GCS ===");

    Application application;

    return app.exec();
}
