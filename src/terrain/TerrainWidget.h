#pragma once

#include <QWidget>
#include <QLabel>

namespace Qt3DCore   { class QEntity; }
namespace Qt3DExtras {
    class Qt3DWindow;
    class QOrbitCameraController;
}

class TerrainScene;

// ─────────────────────────────────────────────────────────────────────────────
// TerrainWidget
// Qt3DWindow + TerrainScene을 묶은 QWidget. Operations 탭에서 사용.
// 씬 빌드/네트워크 로직은 모두 TerrainScene이 담당하고, 본 클래스는
// Qt3D 카메라/조명/카메라컨트롤러 같은 "뷰" 요소만 책임진다.
//
// Mission 탭은 Scene3D + 자체 TerrainScene 인스턴스를 사용하므로
// 본 위젯과 별개로 동작한다.
// ─────────────────────────────────────────────────────────────────────────────
class TerrainWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerrainWidget(QWidget* parent = nullptr);

    void loadTile(double lat, double lon, int zoom = 17);
    void updateVehiclePosition(double lat, double lon);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    QWidget*     _viewport    = nullptr;
    QLabel*      _statusLabel = nullptr;

    Qt3DExtras::Qt3DWindow*             _view        = nullptr;
    Qt3DCore::QEntity*                  _viewRoot    = nullptr;   // Qt3DWindow의 root entity (씬 + 조명 + 카메라컨트롤러 컨테이너)
    Qt3DExtras::QOrbitCameraController* _camCtrl     = nullptr;

    TerrainScene* _scene = nullptr;
    bool _autoLoadDone = false;   // showEvent에서 1회만 자동 로드
};
