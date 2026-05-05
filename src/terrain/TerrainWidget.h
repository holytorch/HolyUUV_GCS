#pragma once

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QMap>
#include "TerrainTile.h"

namespace Qt3DCore   { class QEntity; class QNode; }
namespace Qt3DExtras {
    class Qt3DWindow;
    class QOrbitCameraController;
}
namespace Qt3DRender { class QCamera; class QGeometryRenderer; }

class TerrainWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerrainWidget(QWidget* parent = nullptr);

    void loadTile(double lat, double lon, int zoom = 17);
    int  currentZoom() const { return _zoom; }
    void updateVehiclePosition(double lat, double lon);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void onFetchClicked();
    void onReplyFinished(QNetworkReply* reply);

private:
    // Web Mercator 유틸
    static double metersPerPixel(double lat, int z);
    static double globalPixToLon(double gx, int z);
    static double globalPixToLat(double gy, int z);

    // OSM 타일 처리
    void handleOsmTile(QNetworkReply* reply);
    void stitchAndBuild();   // 스티칭 → 높이 디코딩 → 메시 빌드

    // Qt3D 씬 빌드 (모두 멤버 변수 사용)
    void buildMesh();
    void updateVehicleMarker();
    void resetCameraToTerrain();

    // UI
    QWidget*       _viewport    = nullptr;
    QLabel*        _statusLabel = nullptr;
    QSpinBox*      _zoomSpin    = nullptr;
    QPushButton*   _fetchBtn    = nullptr;

    // Qt3D
    Qt3DExtras::Qt3DWindow*             _view          = nullptr;
    Qt3DCore::QEntity*                  _rootEntity    = nullptr;
    Qt3DCore::QEntity*                  _meshEntity    = nullptr;
    Qt3DCore::QEntity*                  _vehicleMarker = nullptr;
    Qt3DExtras::QOrbitCameraController* _camCtrl       = nullptr;

    // 네트워크
    QNetworkAccessManager _nam;

    // 로드 파라미터
    double _lat  = 35.074857;
    double _lon  = 129.084836;
    int    _zoom = 17;   // OSM 줌 레벨 (13~18)

    // OSM 타일 수신 상태
    int    _osmTX0 = 0, _osmTY0 = 0;   // 스티칭 좌상단 타일
    int    _osmTNX = 0, _osmTNY = 0;   // 타일 개수
    int    _osmPending = 0;
    QMap<QPair<int,int>, QImage> _osmImages;

    // 서브영역 (스티칭 이미지 내 픽셀 좌표)
    int    _stitchCX  = 0, _stitchCY  = 0;  // 중심 픽셀
    int    _subHalfPx = 0;                   // ±픽셀 반경 (3km = ±halfPx)

    // 렌더링 데이터
    TerrainTile _currentTile;
    QString     _osmTexPath;
    static constexpr float _worldHalfSize = 128.0f;

    // 차량 위경도
    double _vehicleLat = 0.0;
    double _vehicleLon = 0.0;
};
