#pragma once

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include "TerrainTile.h"

// Qt3D 전방 선언 (Qt5: QGeometry는 Qt3DRender에 있음)
namespace Qt3DCore   { class QEntity; class QNode; }
namespace Qt3DExtras {
    class Qt3DWindow;
    class QOrbitCameraController;
    class QPhongMaterial;
    class QText2DEntity;
}
namespace Qt3DRender { class QCamera; class QGeometryRenderer; }

// 3D 해저지형 뷰어
// AWS S3 Terrarium 타일 다운로드 → 높이맵 디코딩 → Qt3D 삼각형 메시 생성
// 좌표계: X=동(East), -Z=북(North), Y=위(Up)  → 카메라는 남쪽에서 북쪽을 바라봄
class TerrainWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerrainWidget(QWidget* parent = nullptr);

    // 위경도로 해당 타일 로드
    void loadTile(double lat, double lon, int zoom = 13);

    int currentZoom() const { return _zoom; }

    // 차량 위치 업데이트 (MainWindow에서 GPS 수신 시 호출)
    void updateVehiclePosition(double lat, double lon);

private slots:
    void onFetchClicked();
    void onReplyFinished(QNetworkReply* reply);

private:
    static int    lonToTileX(double lon, int z);
    static int    latToTileY(double lat, int z);
    static double tileYToLat(int ty, int n);

    // 높이 데이터 → Qt3D 격자 메시 생성 (cCol/cRow: 중심 픽셀, halfPx: ±픽셀 반경)
    void buildMesh(const TerrainTile& tile, int cCol, int cRow, int halfPx);

    // 해수면 반투명 평면 생성
    void buildWaterPlane();

    // 나침반 + 4개 모서리 좌표 레이블 생성
    void buildCompass(const TerrainTile& tile, int cCol, int cRow, int halfPx);

    // zoom z, 위도 lat에서의 미터/픽셀
    static double metersPerPixel(double lat, int z);

    // 차량 위치 마커 (빨간 구체) 위치 갱신
    void updateVehicleMarker();

    // UI
    QWidget*       _viewport    = nullptr;  // Qt3DWindow 컨테이너 위젯
    QLabel*        _statusLabel = nullptr;
    QSpinBox*      _zoomSpin    = nullptr;
    QPushButton*   _fetchBtn    = nullptr;

    // Qt3D 씬 루트 및 교체 가능한 자식 엔티티들
    Qt3DExtras::Qt3DWindow*             _view          = nullptr;
    Qt3DCore::QEntity*                  _rootEntity    = nullptr;
    Qt3DCore::QEntity*                  _meshEntity    = nullptr;
    Qt3DCore::QEntity*                  _waterPlane    = nullptr;
    Qt3DCore::QEntity*                  _compassEntity = nullptr;
    Qt3DCore::QEntity*                  _vehicleMarker = nullptr;
    Qt3DExtras::QOrbitCameraController* _camCtrl       = nullptr;

    // 네트워크
    QNetworkAccessManager _nam;

    // 현재 로드 중인 타일 정보 (MapView/GebcoView 기본 중심과 동일)
    double _lat = 35.074857;
    double _lon = 129.084836;
    int    _zoom = 10;
    int    _pendingZ = 0, _pendingX = 0, _pendingY = 0;

    // 마지막으로 로드한 타일 정보
    TerrainTile _currentTile;

    // 현재 렌더링 중인 서브영역 (픽셀 단위)
    int   _meshCenterCol = 128;
    int   _meshCenterRow = 128;
    int   _meshHalfPx    = 4;
    static constexpr float _worldHalfSize = 128.0f;  // 항상 256×256 유닛으로 렌더링

    // 차량 현재 위경도
    double _vehicleLat = 0.0;
    double _vehicleLon = 0.0;
};
