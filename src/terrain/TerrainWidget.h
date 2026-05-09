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

// ─────────────────────────────────────────────────────────────────────────────
// TerrainWidget
// OSM dark_all 타일로 텍스처를, Voyager 타일로 수역 마스크를 만들어
// Qt3D 씬에 3D 지형 메시를 렌더링하는 위젯.
//
// 타일 처리 흐름:
//   loadTile() / Load 버튼
//     → onFetchClicked()       — 필요한 타일 범위 계산, HTTP 요청 시작
//     → handleOsmTile()        — dark_all 타일 수신
//     → handleVoyagerTile()    — Voyager 타일 수신
//     → stitchAndBuild()       — 두 종류 완료 시: 스티칭 → 수역 마스크 → buildMesh()
//
// 수역 판별 (Voyager 픽셀):
//   b > r+15 && b > 150 && g > 150 → SEA_DEPTH(−1000 m)
//   그 외 → LAND_H(+1.0 m)
//
// 높이맵 후처리: 분리형 박스 블러 3회로 해안선 계단 현상 완화
// 법선 계산: 인접 정점 높이차로 per-vertex 법선 → 절벽면 조명 정확
// ─────────────────────────────────────────────────────────────────────────────
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
    void onModeToggled();

private:
    // Web Mercator 좌표 변환 유틸
    static double metersPerPixel(double lat, int z);
    static double globalPixToLon(double gx, int z);
    static double globalPixToLat(double gy, int z);

    // 타일 처리
    void handleOsmTile(QNetworkReply* reply);
    void handleVoyagerTile(QNetworkReply* reply);
    void stitchAndBuild();

    // Qt3D 씬
    void buildMesh();
    void updateVehicleMarker();
    void resetCameraToTerrain();

    // UI 위젯
    QWidget*     _viewport    = nullptr;
    QLabel*      _statusLabel = nullptr;
    QSpinBox*    _zoomSpin    = nullptr;
    QPushButton* _fetchBtn    = nullptr;
    QPushButton* _modeBtn     = nullptr;

    // Qt3D 씬 트리
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
    int    _zoom = 17;

    // dark_all(OSM) 타일 수신 상태
    int    _osmTX0 = 0, _osmTY0 = 0;
    int    _osmTNX = 0, _osmTNY = 0;
    int    _osmPending = 0;
    QMap<QPair<int,int>, QImage> _osmImages;

    // Voyager 타일 수신 상태 (수역 마스크용)
    int    _voyagerPending = 0;
    QMap<QPair<int,int>, QImage> _voyagerImages;

    // 스티칭 이미지 내 서브영역 기준값
    int    _stitchCX  = 0, _stitchCY  = 0;  // 중심 픽셀 좌표
    int    _subHalfPx = 0;                   // 중심에서 ± 픽셀 반경

    // 렌더링 데이터
    TerrainTile _currentTile;
    bool        _isDarkMode  = true;    // true = dark_all 텍스처, false = Voyager 텍스처
    QString     _darkTexPath;           // dark_all 스티칭 PNG 임시 경로
    QString     _lightTexPath;          // Voyager 스티칭 PNG 임시 경로
    QString     _osmTexPath;            // buildMesh()가 현재 사용하는 텍스처 경로
    static constexpr float _worldHalfSize = 128.0f;

    // 차량 위경도
    double _vehicleLat = 0.0;
    double _vehicleLon = 0.0;
};
