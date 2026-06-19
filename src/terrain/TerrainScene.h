#pragma once

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QPair>
#include <QImage>
#include <QString>
#include <climits>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

namespace Qt3DRender { class QCamera; }

// ─────────────────────────────────────────────────────────────────────────────
// TerrainScene
// 마인크래프트 청크 방식의 3D 지형 씬. 타일(청크)마다 개별 메시 엔티티를 두고,
// 로봇이 이동하면 렌더 반경(kRenderDist) 안의 청크만 동적으로 로드하고 벗어난
// 청크는 언로드한다. 모든 청크는 고정된 전역 좌표계(_originPx 기준)에 배치돼
// 로봇이 움직여도 지형이 제자리에 남는다.
//
// 로드 순서: 로봇이 있는 청크 → 전방(heading) 방향 → 나머지 거리순.
//
// 사용:
//   scene->setCamera(cam);
//   scene->attachTo(rootEntity);
//   scene->loadTile(lat, lon, zoom);          // 원점 고정 + 첫 청크 로드
//   scene->updateVehiclePosition(lat, lon);   // 이동 → 마커 + 청크 갱신
//   scene->updateVehicleDepth(depthMeters);   // 수심 → 마커 y
// ─────────────────────────────────────────────────────────────────────────────
class TerrainScene : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt3DCore::QEntity* rootEntity READ rootEntity CONSTANT)
public:
    explicit TerrainScene(QObject* parent = nullptr);
    ~TerrainScene() override;

    Qt3DCore::QEntity* rootEntity() const { return _rootEntity.data(); }
    int                currentZoom() const { return _zoom; }
    Q_INVOKABLE bool   hasTerrainData() const { return !_chunks.isEmpty(); }

    Q_INVOKABLE void setCamera(Qt3DRender::QCamera* cam) { _camera = cam; }
    Q_INVOKABLE void attachTo(Qt3DCore::QEntity* parent);

    Q_INVOKABLE void loadTile(double lat, double lon, int zoom = 17);

    // 다중로봇: sysid별 마커. 차량 텔레메트리 갱신 시 C++(MainWindow)에서 호출.
    void updateVehiclePosition(int sysid, double lat, double lon);
    void updateVehicleDepth(int sysid, double depthMeters);   // 수면 아래 깊이(m, 양수)
    void setVehicleYaw(int sysid, double deg);                // 마커 콘 방향(도, 0=N)
    void removeVehicle(int sysid);                            // 차량 마커 제거

    // 청크 로딩/카메라가 추종할 활성 차량 지정 + 그 차량 heading(전방 우선 로딩용)
    void setActiveSysid(int sysid);
    void setVehicleHeading(double deg) { _vehicleHeading = deg; }

signals:
    void terrainReady();

private slots:
    void _onReplyFinished(QNetworkReply* reply);

private:
    static double metersPerPixel(double lat, int z);
    // 위경도 → 전역 Web Mercator 픽셀 (zoom z 기준)
    static void   latLonToGlobalPx(double lat, double lon, int z, double& gx, double& gy);

    void _updateChunks();                 // 렌더 반경 기준 로드/언로드
    void _requestChunk(int tx, int ty);   // OSM+Voyager 타일 요청
    void _buildChunkMesh(int tx, int ty); // 수신된 타일로 청크 메시 생성
    void _updateMarker(int sysid);        // sysid 마커 생성/위치/회전 갱신
    void _resetCameraToTerrain();
    bool _activePos(double& lat, double& lon) const;  // 활성 차량 위치(있으면)

    // 전역 픽셀 → 월드 좌표
    float _worldX(double globalPx) const { return static_cast<float>(globalPx - _originPxX) * kWorldPerPixel; }
    float _worldZ(double globalPy) const { return static_cast<float>(globalPy - _originPxY) * kWorldPerPixel; }

    QPointer<Qt3DCore::QEntity>    _rootEntity;

    // 다중로봇: sysid별 마커 (콘 엔티티 + 변환 + 최신 포즈). parent=_rootEntity (RAII)
    struct VehicleMarker {
        QPointer<Qt3DCore::QEntity>    entity;
        QPointer<Qt3DCore::QTransform> xform;
        double lat = 0.0, lon = 0.0, depth = 0.0, yaw = 0.0;
        bool   hasPos = false;
    };
    QMap<int, VehicleMarker> _markers;
    int    _activeSysid = 0;        // 청크 로딩/카메라가 추종하는 차량

    Qt3DRender::QCamera* _camera = nullptr;
    QNetworkAccessManager _nam;

    double _lat  = 37.52951029463262;
    double _lon  = 126.94149832867085;
    int    _zoom = 17;

    double _vehicleHeading = 0.0;   // 활성 차량 heading(도, 0=N) — 청크 전방 우선 로딩용
    bool   _firstFixReceived = false;

    // ── 청크 시스템 ──────────────────────────────────────────────────────────
    struct Chunk {
        QPointer<Qt3DCore::QEntity> entity;
        QImage osm;        // 텍스처용 OSM 타일
        QImage voyager;    // 수역 마스크 타일
        bool   osmReady     = false;
        bool   voyagerReady = false;
        bool   built        = false;
    };
    QMap<QPair<int,int>, Chunk> _chunks;   // (tileX, tileY) → 청크

    bool _originSet = false;
    int  _originPxX = 0, _originPxY = 0;       // 전역 좌표 원점(픽셀)
    int  _curTileX  = INT_MIN, _curTileY = INT_MIN;  // 로봇이 마지막으로 있던 타일

    static constexpr float kWorldPerPixel = 0.25f;   // 타일 픽셀당 월드 유닛 (타일 = 64유닛)
    static constexpr float _heightScale   = 0.05f;   // 높이 배율
    static constexpr int   kRenderDist    = 2;       // 청크 렌더 반경 R → (2R+1)² 그리드
    static constexpr int   kChunkQuads    = 32;      // 청크당 격자 quad 수
};
