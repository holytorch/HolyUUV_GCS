#pragma once

#include <QObject>
#include "comm/LinkManager.h"
#include "mavlink/MavlinkManager.h"
#include "vehicle/VehicleState.h"
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
    explicit Application(QObject* parent = nullptr);

    bool initialize();
    int  run();
    void shutdown();

private:
    LinkManager    _linkManager;
    MavlinkManager _mavlinkManager;
    VehicleState   _vehicleState;
    TileCache      _tileCache;
    TileServer     _tileServer{&_tileCache};
    MainWindow     _mainWindow{&_vehicleState};
};
