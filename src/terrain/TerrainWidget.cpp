#include "TerrainWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QDir>
#include <QPainter>
#include <cmath>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseMapMaterial>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DInput/QInputSettings>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>

// TileServer(포트 17777)가 중계하는 로컬 타일 URL 템플릿
// %1 = zoom, %2 = tileX, %3 = tileY
static const QString OSM_URL     = "http://localhost:17777/osm/%1/%2/%3.png";
static const QString VOYAGER_URL = "http://localhost:17777/voyager/%1/%2/%3.png";


// ─────────────────────────────────────────────────────────────────────────────
// metersPerPixel()
// 주어진 위도와 줌 레벨에서 1픽셀에 해당하는 지표 거리(m)를 반환한다.
// 타일 범위를 픽셀 수로 변환할 때 사용한다 (halfPx = 500m / mpp).
// ─────────────────────────────────────────────────────────────────────────────
double TerrainWidget::metersPerPixel(double lat, int z)
{
    return 156543.03392 * std::cos(lat * M_PI / 180.0) / std::pow(2.0, z);
}


// ─────────────────────────────────────────────────────────────────────────────
// globalPixToLon()
// Web Mercator 글로벌 픽셀 X 좌표를 경도(°)로 변환한다.
// 전체 지도 가로 = 256 × 2^z 픽셀, 경도 범위 −180 ~ +180°.
// ─────────────────────────────────────────────────────────────────────────────
double TerrainWidget::globalPixToLon(double gx, int z)
{
    return gx / (256.0 * (1 << z)) * 360.0 - 180.0;
}


// ─────────────────────────────────────────────────────────────────────────────
// globalPixToLat()
// Web Mercator 글로벌 픽셀 Y 좌표를 위도(°)로 변환한다.
// 메르카토르 도법은 위도가 비선형이므로 sinh/atan 변환이 필요하다.
// ─────────────────────────────────────────────────────────────────────────────
double TerrainWidget::globalPixToLat(double gy, int z)
{
    double n = static_cast<double>(1 << z);
    return std::atan(std::sinh(M_PI * (1.0 - 2.0 * gy / (n * 256.0)))) * 180.0 / M_PI;
}


// ─────────────────────────────────────────────────────────────────────────────
// TerrainWidget()
// Qt3D 씬과 UI 컨트롤 바를 구성한다.
//
// 레이아웃:
//   QVBoxLayout
//   ├── _viewport  (Qt3DWindow 컨테이너, 화면 대부분 차지)
//   └── ctrlBar    (Zoom 스핀박스 / Load Terrain 버튼 / Light Mode 버튼 / 상태 레이블)
//
// Qt3D 씬 트리:
//   _rootEntity
//   ├── inputSettings (이벤트 소스 = _view)
//   ├── _camCtrl      (궤도 카메라)
//   ├── lightEntity   (방향성 조명)
//   ├── _meshEntity   (buildMesh() 호출 시 동적 생성·교체)
//   └── _vehicleMarker (updateVehicleMarker() 호출 시 동적 생성·교체)
// ─────────────────────────────────────────────────────────────────────────────
TerrainWidget::TerrainWidget(QWidget* parent)
    : QWidget(parent)
{
    _view = new Qt3DExtras::Qt3DWindow();
    _view->defaultFrameGraph()->setClearColor(QColor(15, 18, 30));

    _viewport = QWidget::createWindowContainer(_view, this);
    _viewport->setMinimumSize(400, 300);
    _viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    _rootEntity = new Qt3DCore::QEntity();
    _view->setRootEntity(_rootEntity);

    auto* inputSettings = new Qt3DInput::QInputSettings(_rootEntity);
    inputSettings->setEventSource(_view);

    Qt3DRender::QCamera* cam = _view->camera();
    cam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    cam->setFieldOfView(60.0f);
    cam->setNearPlane(0.1f);
    cam->setFarPlane(10000.0f);
    cam->setPosition(QVector3D(0, 179, 128));
    cam->setViewCenter(QVector3D(0, 0, 0));
    cam->setUpVector(QVector3D(0, 1, 0));

    _camCtrl = new Qt3DExtras::QOrbitCameraController(_rootEntity);
    _camCtrl->setCamera(cam);
    _camCtrl->setLinearSpeed(-400.0f);
    _camCtrl->setLookSpeed(-180.0f);

    auto* lightEntity = new Qt3DCore::QEntity(_rootEntity);
    auto* light = new Qt3DRender::QDirectionalLight(lightEntity);
    light->setWorldDirection(QVector3D(-0.3f, -1.0f, -0.5f));
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    lightEntity->addComponent(light);

    _statusLabel = new QLabel("Load Terrain 버튼으로 지형 로드", this);
    _statusLabel->setStyleSheet("color: #aaa; font-size: 12px; padding: 2px;");

    _zoomSpin = new QSpinBox(this);
    _zoomSpin->setRange(13, 18);
    _zoomSpin->setValue(17);

    _fetchBtn = new QPushButton("Load Terrain", this);
    connect(_fetchBtn, &QPushButton::clicked, this, &TerrainWidget::onFetchClicked);
    connect(&_nam, &QNetworkAccessManager::finished, this, &TerrainWidget::onReplyFinished);

    _modeBtn = new QPushButton("Light Mode", this);
    connect(_modeBtn, &QPushButton::clicked, this, &TerrainWidget::onModeToggled);

    QHBoxLayout* ctrlBar = new QHBoxLayout();
    ctrlBar->addWidget(new QLabel("Zoom:", this));
    ctrlBar->addWidget(_zoomSpin);
    ctrlBar->addWidget(_fetchBtn);
    ctrlBar->addWidget(_modeBtn);
    ctrlBar->addStretch();
    ctrlBar->addWidget(_statusLabel);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_viewport);
    layout->addLayout(ctrlBar);
    setLayout(layout);
}


// ─────────────────────────────────────────────────────────────────────────────
// showEvent() / hideEvent()
// 탭 전환 시 카메라 컨트롤러를 활성/비활성화한다.
// 숨겨진 탭에서 마우스 이벤트가 카메라에 전달되는 것을 막기 위해서다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(true);
}

void TerrainWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (_camCtrl) _camCtrl->setEnabled(false);
}


// ─────────────────────────────────────────────────────────────────────────────
// loadTile()
// 외부(MainWindow)에서 탭 전환 시 호출된다.
// 위경도·줌을 저장하고 타일 로드를 시작한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::loadTile(double lat, double lon, int zoom)
{
    _lat  = lat;
    _lon  = lon;
    _zoom = zoom;
    _zoomSpin->setValue(zoom);
    onFetchClicked();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateVehiclePosition()
// MAVLink GPS 수신 시 MainWindow가 호출한다.
// 최초 GPS 수신(0,0 → 실제 좌표)이면 맵을 로봇 위치 기준으로 재로드하고,
// 이후에는 마커 위치만 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::updateVehiclePosition(double lat, double lon)
{
    bool firstFix = (_vehicleLat == 0.0 && _vehicleLon == 0.0);
    _vehicleLat = lat;
    _vehicleLon = lon;

    if (firstFix)
        loadTile(lat, lon, _zoom);
    else
        updateVehicleMarker();
}


// ─────────────────────────────────────────────────────────────────────────────
// onFetchClicked()
// Load Terrain 버튼 클릭 또는 loadTile() 호출 시 실행된다.
// 중심 위경도를 Web Mercator 픽셀 좌표로 변환해 필요한 타일 범위를 계산하고
// dark_all · Voyager 타일을 동시에 HTTP 요청한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::onFetchClicked()
{
    _zoom = _zoomSpin->value();

    // 1 km(±500 m) 반경을 픽셀 수로 변환
    double mpp    = metersPerPixel(_lat, _zoom);
    int    halfPx = std::max(2, static_cast<int>(std::ceil(500.0 / mpp)));

    // 중심 위경도 → 글로벌 픽셀 좌표
    double fracX = (_lon + 180.0) / 360.0 * (1 << _zoom);
    double latR  = _lat * M_PI / 180.0;
    double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                   / 2.0 * (1 << _zoom);
    int gCX = static_cast<int>(fracX * 256);
    int gCY = static_cast<int>(fracY * 256);

    // 타일 인덱스 범위
    _osmTX0 = (gCX - halfPx) / 256;
    _osmTY0 = (gCY - halfPx) / 256;
    int tx1 = (gCX + halfPx) / 256;
    int ty1 = (gCY + halfPx) / 256;
    _osmTNX = tx1 - _osmTX0 + 1;
    _osmTNY = ty1 - _osmTY0 + 1;

    // 스티칭 이미지 내 중심 픽셀 좌표
    _stitchCX  = gCX - _osmTX0 * 256;
    _stitchCY  = gCY - _osmTY0 * 256;
    _subHalfPx = halfPx;

    _osmImages.clear();
    _osmPending = _osmTNX * _osmTNY;

    _voyagerImages.clear();
    _voyagerPending = _osmTNX * _osmTNY;

    _statusLabel->setText(QString("OSM+Voyager z=%1 로딩 중... (%2×%3 타일)")
                              .arg(_zoom).arg(_osmTNX).arg(_osmTNY));
    _fetchBtn->setEnabled(false);

    for (int r = 0; r < _osmTNY; ++r) {
        for (int c = 0; c < _osmTNX; ++c) {
            int otx = _osmTX0 + c, oty = _osmTY0 + r;

            auto* osmRep = _nam.get(QNetworkRequest(
                QUrl(OSM_URL.arg(_zoom).arg(otx).arg(oty))));
            osmRep->setProperty("osmTX", otx);
            osmRep->setProperty("osmTY", oty);

            auto* posRep = _nam.get(QNetworkRequest(
                QUrl(VOYAGER_URL.arg(_zoom).arg(otx).arg(oty))));
            posRep->setProperty("positronTX", otx);
            posRep->setProperty("positronTY", oty);
        }
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// onReplyFinished()
// QNetworkAccessManager::finished 신호 핸들러.
// 응답 프로퍼티로 타일 종류를 구분해 각 핸들러로 라우팅한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->property("positronTX").isValid())
        handleVoyagerTile(reply);
    else
        handleOsmTile(reply);
}


// ─────────────────────────────────────────────────────────────────────────────
// handleOsmTile()
// dark_all 타일 수신을 처리한다.
// 성공 시 _osmImages에 저장하고, 두 종류(OSM+Voyager) 모두 완료되면 stitchAndBuild()를 호출한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::handleOsmTile(QNetworkReply* reply)
{
    int otx = reply->property("osmTX").toInt();
    int oty = reply->property("osmTY").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        if (img.loadFromData(reply->readAll()) && !img.isNull()) {
            _osmImages[qMakePair(otx, oty)] =
                img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
            qInfo("OSM tile OK (%d,%d) pending=%d", otx, oty, _osmPending - 1);
        } else {
            qWarning("OSM tile (%d,%d): 이미지 디코딩 실패", otx, oty);
        }
    } else {
        qWarning("OSM tile (%d,%d) error: %s", otx, oty,
                 qPrintable(reply->errorString()));
    }

    if (--_osmPending <= 0 && _voyagerPending <= 0)
        stitchAndBuild();
}


// ─────────────────────────────────────────────────────────────────────────────
// handleVoyagerTile()
// Voyager 타일 수신을 처리한다.
// 성공 시 _voyagerImages에 저장하고, 두 종류 모두 완료되면 stitchAndBuild()를 호출한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::handleVoyagerTile(QNetworkReply* reply)
{
    int otx = reply->property("positronTX").toInt();
    int oty = reply->property("positronTY").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        if (img.loadFromData(reply->readAll()) && !img.isNull()) {
            _voyagerImages[qMakePair(otx, oty)] =
                img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
            qInfo("Voyager tile OK (%d,%d) pending=%d", otx, oty, _voyagerPending - 1);
        } else {
            qWarning("Voyager tile (%d,%d): 이미지 디코딩 실패", otx, oty);
        }
    } else {
        qWarning("Voyager tile (%d,%d) error: %s", otx, oty,
                 qPrintable(reply->errorString()));
    }

    if (--_voyagerPending <= 0 && _osmPending <= 0)
        stitchAndBuild();
}


// ─────────────────────────────────────────────────────────────────────────────
// stitchAndBuild()
// 모든 타일이 수신되면 호출된다. 다음 단계를 순서대로 수행한다:
//   1. dark_all 타일들을 하나의 스티칭 이미지로 합침
//   2. Voyager 타일들을 스티칭해 수역 픽셀 판별 (b > r+15 && b > 150 && g > 150)
//   3. 서브영역 픽셀마다 높이값 결정 (수역 = SEA_DEPTH, 육지 = LAND_H)
//   4. 분리형 박스 블러 3회로 해안선 계단 현상 완화
//   5. 두 스티칭 이미지를 임시 PNG로 저장
//   6. buildMesh() → updateVehicleMarker() → resetCameraToTerrain() 호출
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::stitchAndBuild()
{
    // dark_all 스티칭
    QImage darkStitched(_osmTNX * 256, _osmTNY * 256, QImage::Format_RGB32);
    darkStitched.fill(Qt::black);
    {
        QPainter p(&darkStitched);
        for (int r = 0; r < _osmTNY; ++r)
            for (int c = 0; c < _osmTNX; ++c) {
                auto key = qMakePair(_osmTX0 + c, _osmTY0 + r);
                if (_osmImages.contains(key))
                    p.drawImage(c * 256, r * 256, _osmImages[key]);
            }
    }

    // Voyager 스티칭 (수역 마스크용)
    QImage lightStitched(_osmTNX * 256, _osmTNY * 256, QImage::Format_RGB32);
    lightStitched.fill(Qt::white);
    {
        QPainter p(&lightStitched);
        for (int r = 0; r < _osmTNY; ++r)
            for (int c = 0; c < _osmTNX; ++c) {
                auto key = qMakePair(_osmTX0 + c, _osmTY0 + r);
                if (_voyagerImages.contains(key))
                    p.drawImage(c * 256, r * 256, _voyagerImages[key]);
            }
    }

    const int   subW      = _subHalfPx * 2 + 1;
    const int   subH      = _subHalfPx * 2 + 1;
    const float SEA_DEPTH = -1000.0f;
    const float LAND_H    = 1.0f;

    _currentTile.tileZ  = _zoom;
    _currentTile.tileX  = _osmTX0;
    _currentTile.tileY  = _osmTY0;
    _currentTile.width  = subW;
    _currentTile.height = subH;
    _currentTile.heights.resize(subW * subH);

    // Voyager 샘플 픽셀 로그 (수역 감지 디버깅용)
    int waterCount = 0, landCount = 0;
    auto logPx = [&](int px, int py, const char* label) {
        QRgb c = lightStitched.pixel(
            qBound(0, px, lightStitched.width()  - 1),
            qBound(0, py, lightStitched.height() - 1));
        qInfo("Voyager [%s] RGB: (%d, %d, %d)", label, qRed(c), qGreen(c), qBlue(c));
    };
    logPx(_stitchCX,                  _stitchCY,                  "중심");
    logPx(_stitchCX - _subHalfPx / 2, _stitchCY,                  "서쪽");
    logPx(_stitchCX + _subHalfPx / 2, _stitchCY,                  "동쪽");
    logPx(_stitchCX,                  _stitchCY - _subHalfPx / 2, "북쪽");
    logPx(_stitchCX,                  _stitchCY + _subHalfPx / 2, "남쪽");

    int blueLoose = 0;
    for (int py = 0; py < lightStitched.height(); ++py)
        for (int px = 0; px < lightStitched.width(); ++px) {
            QRgb c = lightStitched.pixel(px, py);
            if (qBlue(c) > qRed(c) + 5) ++blueLoose;
        }
    qInfo("파랑 우세 픽셀(b>r+5): %d / %d",
          blueLoose, lightStitched.width() * lightStitched.height());

    // 수역 판별: 선명한 파란 픽셀 → SEA_DEPTH, 나머지 → LAND_H
    for (int sy = 0; sy < subH; ++sy) {
        for (int sx = 0; sx < subW; ++sx) {
            int  px  = qBound(0, _stitchCX - _subHalfPx + sx, lightStitched.width()  - 1);
            int  py  = qBound(0, _stitchCY - _subHalfPx + sy, lightStitched.height() - 1);
            QRgb col = lightStitched.pixel(px, py);
            int  r = qRed(col), g = qGreen(col), b = qBlue(col);
            bool water = (b > r + 15) && (b > 150) && (g > 150);
            _currentTile.heights[sy * subW + sx] = water ? SEA_DEPTH : LAND_H;
            water ? ++waterCount : ++landCount;
        }
    }
    qInfo("수역 판별 결과: water=%d / land=%d (총 %d픽셀)",
          waterCount, landCount, subW * subH);

    // 분리형 박스 블러 3회 — 해안선 픽셀 계단을 완만한 경사면으로 변환
    constexpr int BLUR_RADIUS = 1;
    constexpr int BLUR_PASS   = 3;
    std::vector<float> tmp(subW * subH);
    auto& h = _currentTile.heights;
    for (int pass = 0; pass < BLUR_PASS; ++pass) {
        for (int sy = 0; sy < subH; ++sy) {
            for (int sx = 0; sx < subW; ++sx) {
                float sum = 0.f; int cnt = 0;
                for (int d = -BLUR_RADIUS; d <= BLUR_RADIUS; ++d) {
                    int nx = qBound(0, sx + d, subW - 1);
                    sum += h[sy * subW + nx]; ++cnt;
                }
                tmp[sy * subW + sx] = sum / cnt;
            }
        }
        for (int sy = 0; sy < subH; ++sy) {
            for (int sx = 0; sx < subW; ++sx) {
                float sum = 0.f; int cnt = 0;
                for (int d = -BLUR_RADIUS; d <= BLUR_RADIUS; ++d) {
                    int ny = qBound(0, sy + d, subH - 1);
                    sum += tmp[ny * subW + sx]; ++cnt;
                }
                h[sy * subW + sx] = sum / cnt;
            }
        }
    }

    // 스티칭 이미지를 임시 PNG로 저장 (Qt3D QTextureImage는 파일 경로로 로드)
    static int seq = 0;
    ++seq;
    _darkTexPath  = QString("%1/holyuuv_dark_%2.png").arg(QDir::tempPath()).arg(seq);
    _lightTexPath = QString("%1/holyuuv_light_%2.png").arg(QDir::tempPath()).arg(seq);

    bool darkSaved  = darkStitched.save(_darkTexPath);
    bool lightSaved = lightStitched.save(_lightTexPath);
    qInfo("텍스처 저장: dark=%s (%s) light=%s (%s)",
          darkSaved  ? "OK" : "FAIL", qPrintable(_darkTexPath),
          lightSaved ? "OK" : "FAIL", qPrintable(_lightTexPath));

    _osmTexPath = _isDarkMode ? _darkTexPath : _lightTexPath;

    buildMesh();
    updateVehicleMarker();
    resetCameraToTerrain();

    _fetchBtn->setEnabled(true);
    _statusLabel->setText(
        QString("OSM z=%1  lat=%2 lon=%3  %4×%5px  ~%6m/px")
            .arg(_zoom)
            .arg(_lat, 0, 'f', 5)
            .arg(_lon, 0, 'f', 5)
            .arg(subW).arg(subH)
            .arg(metersPerPixel(_lat, _zoom), 0, 'f', 1));
}


// ─────────────────────────────────────────────────────────────────────────────
// onModeToggled()
// 다크/라이트 모드 토글 버튼 클릭 처리.
// 텍스처 경로만 교체하고 buildMesh()를 다시 호출한다 (타일 재다운로드 없음).
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::onModeToggled()
{
    _isDarkMode = !_isDarkMode;
    _modeBtn->setText(_isDarkMode ? "Light Mode" : "Dark Mode");

    if (_darkTexPath.isEmpty() || _lightTexPath.isEmpty()) return;

    _osmTexPath = _isDarkMode ? _darkTexPath : _lightTexPath;
    buildMesh();
    updateVehicleMarker();
}


// ─────────────────────────────────────────────────────────────────────────────
// buildMesh()
// _currentTile 높이맵과 _osmTexPath 텍스처로 Qt3D 지형 메시를 생성한다.
//
// 정점 포맷 (float 8개/정점):
//   [0..2] position  (x, y, z)  — y = 높이
//   [3..5] normal    (nx, ny, nz) — 인접 정점 높이차로 계산한 per-vertex 법선
//   [6..7] uv        (u, v)      — 스티칭 이미지 전체 기준 텍스처 좌표
//
// 인덱스: (subW−1)×(subH−1)개 사각형 → 각각 삼각형 2개(6 인덱스)
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::buildMesh()
{
    if (_meshEntity) {
        _meshEntity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _meshEntity;
        _meshEntity = nullptr;
    }

    const TerrainTile& tile = _currentTile;
    const int   subW        = tile.width;
    const int   subH        = tile.height;
    const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
    const float heightScale = 0.05f;

    const float texW  = static_cast<float>(_osmTNX * 256);
    const float texH  = static_cast<float>(_osmTNY * 256);
    const int   imgX0 = _stitchCX - _subHalfPx;
    const int   imgY0 = _stitchCY - _subHalfPx;

    QByteArray vertexBytes(subW * subH * 8 * sizeof(float), Qt::Uninitialized);
    float* vp = reinterpret_cast<float*>(vertexBytes.data());

    for (int sr = 0; sr < subH; ++sr) {
        for (int sc = 0; sc < subW; ++sc) {
            float h = tile.heightAt(sc, sr);

            *vp++ = (sc - _subHalfPx) * scale;
            *vp++ = h * heightScale;
            *vp++ = (sr - _subHalfPx) * scale;

            // per-vertex 법선: 인접 정점 높이차의 교차곱
            int sc0 = std::max(0, sc - 1), sc1 = std::min(subW - 1, sc + 1);
            int sr0 = std::max(0, sr - 1), sr1 = std::min(subH - 1, sr + 1);
            float dx   = (sc1 - sc0) * scale;
            float dz   = (sr1 - sr0) * scale;
            float dy_x = (tile.heightAt(sc1, sr) - tile.heightAt(sc0, sr)) * heightScale;
            float dy_z = (tile.heightAt(sc, sr1) - tile.heightAt(sc, sr0)) * heightScale;
            float nx = -dy_x * dz;
            float ny =  dx * dz;
            float nz = -dy_z * dx;
            float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nlen > 1e-6f) { nx /= nlen; ny /= nlen; nz /= nlen; }
            else               { nx = 0.f;  ny = 1.f;   nz = 0.f; }
            *vp++ = nx; *vp++ = ny; *vp++ = nz;

            *vp++ = (imgX0 + sc) / texW;
            *vp++ = (imgY0 + sr) / texH;
        }
    }

    const int quadCount = (subW - 1) * (subH - 1);
    QByteArray indexBytes(quadCount * 6 * sizeof(uint32_t), Qt::Uninitialized);
    uint32_t* ip = reinterpret_cast<uint32_t*>(indexBytes.data());
    for (int sr = 0; sr < subH - 1; ++sr) {
        for (int sc = 0; sc < subW - 1; ++sc) {
            uint32_t tl = static_cast<uint32_t>(sr * subW + sc);
            uint32_t tr = tl + 1;
            uint32_t bl = tl + static_cast<uint32_t>(subW);
            uint32_t br = bl + 1;
            *ip++ = tl; *ip++ = bl; *ip++ = tr;
            *ip++ = tr; *ip++ = bl; *ip++ = br;
        }
    }

    auto* geometry = new Qt3DRender::QGeometry();
    auto* vBuf = new Qt3DRender::QBuffer(geometry);
    vBuf->setData(vertexBytes);
    auto* iBuf = new Qt3DRender::QBuffer(geometry);
    iBuf->setData(indexBytes);

    auto addAttr = [&](const QString& name, int offset, int size) {
        auto* a = new Qt3DRender::QAttribute(geometry);
        a->setName(name);
        a->setVertexBaseType(Qt3DRender::QAttribute::Float);
        a->setVertexSize(size);
        a->setByteOffset(offset * sizeof(float));
        a->setByteStride(8 * sizeof(float));
        a->setCount(static_cast<uint>(subW * subH));
        a->setBuffer(vBuf);
        geometry->addAttribute(a);
    };
    addAttr(Qt3DRender::QAttribute::defaultPositionAttributeName(),          0, 3);
    addAttr(Qt3DRender::QAttribute::defaultNormalAttributeName(),            3, 3);
    addAttr(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(), 6, 2);

    auto* idxAttr = new Qt3DRender::QAttribute(geometry);
    idxAttr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    idxAttr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    idxAttr->setCount(static_cast<uint>(quadCount * 6));
    idxAttr->setBuffer(iBuf);
    geometry->addAttribute(idxAttr);

    auto* renderer = new Qt3DRender::QGeometryRenderer();
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    qInfo("buildMesh: 텍스처 경로=%s", qPrintable(_osmTexPath));
    auto* tex = new Qt3DRender::QTexture2D(_rootEntity);
    tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    auto* texImg = new Qt3DRender::QTextureImage(tex);
    texImg->setSource(QUrl::fromLocalFile(_osmTexPath));
    texImg->setMirrored(false);
    tex->addTextureImage(texImg);

    auto* mat = new Qt3DExtras::QDiffuseMapMaterial(_rootEntity);
    mat->setDiffuse(tex);
    mat->setAmbient(QColor(180, 180, 180));
    mat->setSpecular(QColor(20, 20, 20));
    mat->setShininess(5.0f);

    _meshEntity = new Qt3DCore::QEntity(_rootEntity);
    _meshEntity->addComponent(renderer);
    _meshEntity->addComponent(mat);
}


// ─────────────────────────────────────────────────────────────────────────────
// updateVehicleMarker()
// 차량 위경도를 3D 월드 좌표로 변환해 빨간 구체 마커를 배치한다.
// 차량이 현재 맵 서브영역 밖에 있으면 마커를 표시하지 않는다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::updateVehicleMarker()
{
    if (_vehicleLat == 0.0 && _vehicleLon == 0.0) return;
    if (!_currentTile.isValid()) return;

    if (_vehicleMarker) {
        _vehicleMarker->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _vehicleMarker;
        _vehicleMarker = nullptr;
    }

    double fracX = (_vehicleLon + 180.0) / 360.0 * (1 << _zoom);
    double latR  = _vehicleLat * M_PI / 180.0;
    double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                   / 2.0 * (1 << _zoom);
    double gVX = fracX * 256;
    double gVY = fracY * 256;

    double sVX = gVX - _osmTX0 * 256;
    double sVY = gVY - _osmTY0 * 256;

    double relX = sVX - _stitchCX;
    double relY = sVY - _stitchCY;

    if (std::abs(relX) > _subHalfPx || std::abs(relY) > _subHalfPx) return;

    const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
    const float heightScale = 0.05f;

    int   sc   = static_cast<int>(relX + _subHalfPx);
    int   sr   = static_cast<int>(relY + _subHalfPx);
    float h    = _currentTile.heightAt(sc, sr);
    float xPos = static_cast<float>(relX) * scale;
    float zPos = static_cast<float>(relY) * scale;
    float yPos = h * heightScale + 8.0f;

    _vehicleMarker = new Qt3DCore::QEntity(_rootEntity);
    auto* sphere = new Qt3DExtras::QSphereMesh(_vehicleMarker);
    sphere->setRadius(5.0f);
    sphere->setRings(8);
    sphere->setSlices(8);

    auto* mat = new Qt3DExtras::QPhongMaterial(_vehicleMarker);
    mat->setAmbient(QColor(200, 20, 20));
    mat->setDiffuse(QColor(255, 60, 60));
    mat->setSpecular(QColor(255, 200, 200));
    mat->setShininess(60.0f);

    auto* t = new Qt3DCore::QTransform();
    t->setTranslation(QVector3D(xPos, yPos, zPos));

    _vehicleMarker->addComponent(sphere);
    _vehicleMarker->addComponent(mat);
    _vehicleMarker->addComponent(t);
}


// ─────────────────────────────────────────────────────────────────────────────
// resetCameraToTerrain()
// 타일 로드 완료 후 초기 카메라 시점을 설정한다.
//
// 타겟 결정 우선순위:
//   1. 차량 마커가 맵 위에 있음 → 차량 3D 위치
//   2. GPS 없음 또는 맵 밖       → 맵 중심 (0, 0, 0)
//
// 카메라 위치: 타겟에서 북향(-Z), 약 20° 아래를 바라보도록
// 700 m 위, 500 m 남쪽에 배치 (월드 유닛 변환 적용).
// ─────────────────────────────────────────────────────────────────────────────
void TerrainWidget::resetCameraToTerrain()
{
    Qt3DRender::QCamera* cam = _view->camera();
    QVector3D target(0.0f, 0.0f, 0.0f);

    if (_vehicleMarker && _currentTile.isValid()) {
        double fracX = (_vehicleLon + 180.0) / 360.0 * (1 << _zoom);
        double latR  = _vehicleLat * M_PI / 180.0;
        double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                       / 2.0 * (1 << _zoom);
        double relX  = (fracX * 256 - _osmTX0 * 256) - _stitchCX;
        double relY  = (fracY * 256 - _osmTY0 * 256) - _stitchCY;

        const float scale       = _worldHalfSize / static_cast<float>(_subHalfPx);
        const float heightScale = 0.05f;
        int   sc = static_cast<int>(relX + _subHalfPx);
        int   sr = static_cast<int>(relY + _subHalfPx);
        float h  = _currentTile.heightAt(sc, sr);

        target = QVector3D(static_cast<float>(relX) * scale,
                           h * heightScale,
                           static_cast<float>(relY) * scale);
    }

    float mpp          = static_cast<float>(metersPerPixel(_lat, _zoom));
    float unitsPerMeter = _worldHalfSize / (_subHalfPx * mpp);

    float up    = 700.0f * unitsPerMeter;
    float south = 500.0f * unitsPerMeter;
    cam->setPosition(target + QVector3D(0.0f, up, south));
    cam->setViewCenter(target);
    cam->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
}
