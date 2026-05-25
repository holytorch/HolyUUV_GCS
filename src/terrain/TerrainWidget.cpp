#include "TerrainWidget.h"
#include "TerrainScene.h"

#include <QVBoxLayout>
#include <QLabel>

#include <Qt3DCore/QEntity>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DInput/QInputSettings>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QCamera>


// ─────────────────────────────────────────────────────────────────────────────
// TerrainWidget()
// Qt3DWindow + 조명 + 카메라컨트롤러를 구성하고, TerrainScene을 만들어
// 씬의 root entity를 Qt3DWindow의 root에 reparent한다.
//
// Qt3D 씬 트리:
//   _viewRoot                 (Qt3DWindow의 root)
//   ├── inputSettings
//   ├── _camCtrl
//   ├── lightEntity
//   └── _scene->rootEntity()  (메시, 차량마커 — TerrainScene이 동적 관리)
// ─────────────────────────────────────────────────────────────────────────────
TerrainWidget::TerrainWidget(QWidget* parent)
    : QWidget(parent)
{
    _view = new Qt3DExtras::Qt3DWindow();
    _view->defaultFrameGraph()->setClearColor(QColor(15, 18, 30));

    _viewport = QWidget::createWindowContainer(_view, this);
    _viewport->setMinimumSize(400, 300);
    _viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    _viewRoot = new Qt3DCore::QEntity();
    _view->setRootEntity(_viewRoot);

    auto* inputSettings = new Qt3DInput::QInputSettings(_viewRoot);
    inputSettings->setEventSource(_view);

    Qt3DRender::QCamera* cam = _view->camera();
    cam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    cam->setFieldOfView(60.0f);
    cam->setNearPlane(0.1f);
    cam->setFarPlane(10000.0f);
    cam->setPosition(QVector3D(0, 179, 128));
    cam->setViewCenter(QVector3D(0, 0, 0));
    cam->setUpVector(QVector3D(0, 1, 0));

    _camCtrl = new Qt3DExtras::QOrbitCameraController(_viewRoot);
    _camCtrl->setCamera(cam);
    _camCtrl->setLinearSpeed(-400.0f);
    _camCtrl->setLookSpeed(-180.0f);

    auto* lightEntity = new Qt3DCore::QEntity(_viewRoot);
    auto* light = new Qt3DRender::QDirectionalLight(lightEntity);
    light->setWorldDirection(QVector3D(-0.3f, -1.0f, -0.5f));
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    lightEntity->addComponent(light);

    // 씬 빌더 생성. rootEntity를 _viewRoot의 자식으로 reparent.
    _scene = new TerrainScene(this);
    _scene->setCamera(cam);
    _scene->attachTo(_viewRoot);

    // 상태 레이블 (자동 로드 — 표시되면 자동 fetch)
    _statusLabel = new QLabel("", this);
    _statusLabel->setStyleSheet("color: #aaa; font-size: 11px; padding: 2px; background: rgba(0,0,0,0.4);");
    _statusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_viewport);
    setLayout(layout);
}


// 탭 전환 시 카메라 컨트롤러 활성/비활성. 첫 표시 때만 fetch 트리거.
// (생성자 시점에는 TileServer가 아직 listening 안 했을 수 있어 race condition)
void TerrainWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(true);

    if (!_autoLoadDone) {
        _autoLoadDone = true;
        _scene->loadTile(35.074857, 129.084836, 17);
    }
}

void TerrainWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(false);
}


void TerrainWidget::loadTile(double lat, double lon, int zoom)
{
    if (_scene) _scene->loadTile(lat, lon, zoom);
}


void TerrainWidget::updateVehiclePosition(double lat, double lon)
{
    if (_scene) _scene->updateVehiclePosition(lat, lon);
}
