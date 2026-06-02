#pragma once

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QImage>
#include <QString>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include "TerrainTile.h"

namespace Qt3DRender { class QCamera; }

// ─────────────────────────────────────────────────────────────────────────────
// TerrainScene
// Qt3D 지형 씬을 빌드/관리하는 ViewModel-격 클래스 (QObject 기반).
// 어떤 QWidget/QQuickItem에도 묶이지 않으며, _rootEntity를 외부 부모
// (Qt3DWindow의 root entity OR QML Scene3D Entity)에 reparent해서 사용.
//
// TerrainWidget(QWidget+Qt3DWindow) 과 Mission 탭(QML Scene3D)가
// 각자 자기 TerrainScene 인스턴스를 만들어 사용한다. 타일은 양쪽이
// 독립적으로 fetch하지만 TileServer 캐시를 공유하므로 비용은 작다.
//
// 사용:
//   auto* scene = new TerrainScene(this);
//   scene->setCamera(qt3dwindow->camera());        // resetCameraToTerrain 용
//   scene->attachTo(qt3dwindow->rootEntity());     // 또는 QML Entity
//   scene->loadTile(lat, lon, zoom);
//   scene->updateVehiclePosition(lat, lon);
// ─────────────────────────────────────────────────────────────────────────────
class TerrainScene : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt3DCore::QEntity* rootEntity READ rootEntity CONSTANT)
public:
    explicit TerrainScene(QObject* parent = nullptr);
    ~TerrainScene() override;

    Qt3DCore::QEntity* rootEntity() const { return _rootEntity.data(); }
    int                currentZoom() const { return _zoom; }
    Q_INVOKABLE bool   hasTerrainData() const { return _currentTile.isValid(); }

    // 카메라 포인터를 외부에서 주입. resetCameraToTerrain()이 사용.
    // QML Scene3D에서는 QCamera 객체를 setCamera로 넘긴다.
    Q_INVOKABLE void setCamera(Qt3DRender::QCamera* cam) { _camera = cam; }

    // 외부 부모 Entity에 _rootEntity를 reparent. QML에서 호출하기 위해 Q_INVOKABLE.
    Q_INVOKABLE void attachTo(Qt3DCore::QEntity* parent);

    Q_INVOKABLE void loadTile(double lat, double lon, int zoom = 17);
    Q_INVOKABLE void updateVehiclePosition(double lat, double lon);

signals:
    // stitchAndBuild()까지 마쳐서 mesh가 렌더링 가능한 상태가 되면 emit.
    void terrainReady();

private slots:
    void _onReplyFinished(QNetworkReply* reply);

private:
    // Web Mercator 변환 유틸
    static double metersPerPixel(double lat, int z);
    static double globalPixToLon(double gx, int z);
    static double globalPixToLat(double gy, int z);

    void _startFetch();
    void _handleOsmTile(QNetworkReply* reply);
    void _handleVoyagerTile(QNetworkReply* reply);
    void _stitchAndBuild();

    struct WorldPos {
        bool  inBounds = false;
        float x        = 0.0f;
        float z        = 0.0f;
        float terrainH = 0.0f;
    };
    WorldPos _latLonToWorld(double lat, double lon) const;

    void _buildMesh();
    void _updateVehicleMarker();
    void _resetCameraToTerrain();

    // Entity 트리. QPointer로 보호 — QML Scene3D가 destroy되면
    // 그 자식인 _rootEntity가 함께 destroy될 수 있으므로 자동 null-out 필요.
    QPointer<Qt3DCore::QEntity>    _rootEntity;
    QPointer<Qt3DCore::QEntity>    _meshEntity;
    QPointer<Qt3DCore::QEntity>    _vehicleMarker;
    QPointer<Qt3DCore::QTransform> _vehicleXform;

    // 외부 주입 카메라 (TerrainScene이 소유 안 함)
    Qt3DRender::QCamera* _camera = nullptr;

    // 네트워크
    QNetworkAccessManager _nam;

    // 로드 파라미터
    double _lat  = 37.52951029463262;
    double _lon  = 126.94149832867085;
    int    _zoom = 17;

    // OSM(dark_all) 수신 상태
    int    _osmTX0 = 0, _osmTY0 = 0;
    int    _osmTNX = 0, _osmTNY = 0;
    int    _osmPending = 0;
    QMap<QPair<int,int>, QImage> _osmImages;

    // Voyager 수신 상태 (수역 마스크)
    int    _voyagerPending = 0;
    QMap<QPair<int,int>, QImage> _voyagerImages;

    // 스티칭 이미지 좌표
    int    _stitchCX  = 0, _stitchCY  = 0;
    int    _subHalfPx = 0;

    // 렌더 데이터
    TerrainTile _currentTile;
    QString     _darkTexPath;
    QString     _osmTexPath;

    // 월드 스케일
    static constexpr float _worldHalfSize = 128.0f;
    static constexpr float _heightScale   = 0.05f;

    // 차량 위치
    double _vehicleLat = 0.0;
    double _vehicleLon = 0.0;
    bool   _firstFixReceived = false;
};
