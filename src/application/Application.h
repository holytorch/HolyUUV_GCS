#pragma once

#include <QObject>
#include "comm/LinkManager.h"
#include "mavlink/MavlinkManager.h"
#include "vehicle/VehicleState.h"
#include "vehicle/VehicleManager.h"
#include "ui/LogFeed.h"
#include "ui/MainWindow.h"
#include "map/TileCache.h"
#include "map/TileServer.h"

// ─────────────────────────────────────────────────────────────────────────────
// Application
// GCS의 최상위 컨트롤러. 모든 하위 시스템(통신·MAVLink·차량 상태·타일·UI)을
// 소유하고 시그널/슬롯으로 연결한다.
//
// 실행 흐름:
//   main()
//   └─ Application::initialize()  — 시스템 연결, 링크 감지, 창 표시
//   └─ Application::run()         — Qt 이벤트 루프 진입 (블로킹)
//   └─ Application::shutdown()    — 종료 정리
//
// 소유 구조:
//   Application
//   ├── LinkManager    — Serial / UDP 링크 전환 및 바이트 수신
//   ├── MavlinkManager — 바이트 스트림 → MAVLink 메시지 파싱
//   ├── VehicleState   — 파싱된 텔레메트리 저장 및 변경 알림
//   ├── TileCache      — SQLite 타일 캐시 (5 GB LRU)
//   ├── TileServer     — 로컬 HTTP 타일 프록시 (127.0.0.1:17777)
//   └── MainWindow     — 메인 UI 창
// ─────────────────────────────────────────────────────────────────────────────
class Application : public QObject {
    Q_OBJECT
public:
    // explicit 의도한 생성자만 허용 (예: 부모 QObject*), 암시적 변환 방지
    //QObject : 부모, Application : 상속받은 자식
    explicit Application(QObject* parent = nullptr);

    bool initialize();
    int  run();
    void shutdown();

private:
    // Application이 생성될 때, 자동으로 같이 생성자 실행됨.
    // _logFeed를 가장 먼저 선언 → 가장 먼저 생성되어 메시지 핸들러를 설치하고,
    // 이후 모든 객체의 [init]/[exit] 로그를 인앱 피드까지 캡처한다.
    LogFeed        _logFeed;
    LinkManager    _linkManager;
    MavlinkManager _mavlinkManager;
    VehicleState   _vehicleState;
    // 다중로봇 단일 진실원천 (마이그레이션 중 _vehicleState와 병렬로 도입; 라우팅은 다음 단계)
    VehicleManager _vehicleManager;
    TileCache      _tileCache;
    TileServer     _tileServer{&_tileCache};
    MainWindow     _mainWindow{&_vehicleState, &_logFeed};
};
