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
#include <cmath>

// Qt3D
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QText2DEntity>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QConeMesh>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>

// ─────────────────────────────────────────────
// 좌표계 정의:
//   X  = 동쪽 (East)   +X → 오른쪽
//  -Z  = 북쪽 (North)  -Z → 화면 안쪽 (카메라 기본 시선 방향)
//   Y  = 위  (Up)
//
// 타일 픽셀 → 3D 좌표 변환:
//   col 0   = 서쪽(West)  → X = -halfW
//   col W-1 = 동쪽(East)  → X = +halfW
//   row 0   = 북쪽(North) → Z = -halfH   (타일 row0이 북쪽)
//   row H-1 = 남쪽(South) → Z = +halfH
// ─────────────────────────────────────────────

// GEBCO 색상 기반 수심 추정 (Terrarium 대체)
static const QString TERRAIN_URL = "http://127.0.0.1:17777/gebco/%1/%2/%3.png";

// zoom z, 위도 lat(도)에서의 미터/픽셀 (Web Mercator)
double TerrainWidget::metersPerPixel(double lat, int z)
{
    return 156543.03392 * std::cos(lat * M_PI / 180.0) / std::pow(2.0, z);
}

// ─────────────────────────────────────────────
// 생성자
// ─────────────────────────────────────────────
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

    // ── 카메라: 남쪽(+Z)에서 높이를 두고 북쪽(-Z)을 바라봄 ──
    // 지도와 같은 방향: 화면 위쪽 = 북쪽
    Qt3DRender::QCamera* cam = _view->camera();
    cam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    cam->setFieldOfView(60.0f);
    cam->setNearPlane(0.1f);
    cam->setFarPlane(10000.0f);
    // 남동쪽 상공에서 북서쪽 바다를 사선으로 내려다보는 시점
    cam->setPosition(QVector3D(60, 160, 220));
    cam->setViewCenter(QVector3D(0, -10, -30));
    cam->setUpVector(QVector3D(0, 1, 0));

    _camCtrl = new Qt3DExtras::QOrbitCameraController(_rootEntity);
    _camCtrl->setCamera(cam);
    _camCtrl->setLinearSpeed(400.0f);
    _camCtrl->setLookSpeed(180.0f);

    // 씬 조명: 나침반 화살표 등 PhongMaterial 오브젝트가 보이도록
    auto* lightEntity = new Qt3DCore::QEntity(_rootEntity);
    auto* light = new Qt3DRender::QDirectionalLight(lightEntity);
    light->setWorldDirection(QVector3D(-0.3f, -1.0f, -0.5f));
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    lightEntity->addComponent(light);

    // ── 컨트롤 바 ────────────────────────────────
    _statusLabel = new QLabel("Load Terrain 버튼으로 지형 로드", this);
    _statusLabel->setStyleSheet("color: #aaa; font-size: 12px; padding: 2px;");

    _zoomSpin = new QSpinBox(this);
    _zoomSpin->setRange(1, 10);   // ESRI Ocean Base 최대 줌 10 (= 10.9)
    _zoomSpin->setValue(10);

    _fetchBtn = new QPushButton("Load Terrain", this);
    connect(_fetchBtn, &QPushButton::clicked, this, &TerrainWidget::onFetchClicked);
    connect(&_nam, &QNetworkAccessManager::finished, this, &TerrainWidget::onReplyFinished);

    QHBoxLayout* ctrlBar = new QHBoxLayout();
    ctrlBar->addWidget(new QLabel("Zoom:", this));
    ctrlBar->addWidget(_zoomSpin);
    ctrlBar->addWidget(_fetchBtn);
    ctrlBar->addStretch();
    ctrlBar->addWidget(_statusLabel);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_viewport);
    layout->addLayout(ctrlBar);
    setLayout(layout);
}

// ─────────────────────────────────────────────
// 공개 API
// ─────────────────────────────────────────────
void TerrainWidget::loadTile(double lat, double lon, int zoom)
{
    _lat  = lat;
    _lon  = lon;
    _zoom = zoom;
    _zoomSpin->setValue(zoom);
    onFetchClicked();
}

void TerrainWidget::updateVehiclePosition(double lat, double lon)
{
    _vehicleLat = lat;
    _vehicleLon = lon;
    updateVehicleMarker();
}

// ─────────────────────────────────────────────
// Load 버튼 → TileServer HTTP 요청
// ─────────────────────────────────────────────
void TerrainWidget::onFetchClicked()
{
    _zoom     = _zoomSpin->value();
    _pendingZ = _zoom;
    _pendingX = lonToTileX(_lon, _zoom);
    _pendingY = latToTileY(_lat, _zoom);

    _statusLabel->setText(QString("Fetching z=%1 x=%2 y=%3...")
                              .arg(_pendingZ).arg(_pendingX).arg(_pendingY));
    QNetworkRequest req(TERRAIN_URL.arg(_pendingZ).arg(_pendingX).arg(_pendingY));
    req.setHeader(QNetworkRequest::UserAgentHeader, "HolyUUV_GCS/1.0");
    _nam.get(req);
    _fetchBtn->setEnabled(false);
}

// ─────────────────────────────────────────────
// HTTP 응답 → 디코딩 → 메시 빌드
// ─────────────────────────────────────────────
void TerrainWidget::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    _fetchBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        _statusLabel->setText("Error: " + reply->errorString());
        return;
    }

    TerrainTile tile = TerrainTile::decodeGebco(_pendingZ, _pendingX, _pendingY, reply->readAll());
    if (!tile.isValid()) {
        _statusLabel->setText("PNG 디코딩 실패");
        return;
    }

    _currentTile = tile;

    // ── 1km×1km 서브영역 계산 ─────────────────────────
    // zoom 10, lat 35° → ~125m/pixel → 1km = ±4픽셀
    double mpp    = metersPerPixel(_lat, tile.tileZ);
    int    halfPx = std::max(2, static_cast<int>(std::ceil(1500.0 / mpp)));

    // 로봇 위치 기반 중심 (타일 내 없으면 타일 중심)
    int cCol = tile.width  / 2;
    int cRow = tile.height / 2;
    if (_vehicleLat != 0.0 || _vehicleLon != 0.0) {
        const int n = 1 << tile.tileZ;
        double lonW  = static_cast<double>(tile.tileX)     / n * 360.0 - 180.0;
        double lonE  = static_cast<double>(tile.tileX + 1) / n * 360.0 - 180.0;
        double latN  = tileYToLat(tile.tileY,     n);
        double latS  = tileYToLat(tile.tileY + 1, n);
        double relX  = (_vehicleLon - lonW) / (lonE - lonW);
        double relY  = (latN - _vehicleLat) / (latN - latS);
        if (relX >= 0.0 && relX <= 1.0 && relY >= 0.0 && relY <= 1.0) {
            cCol = static_cast<int>(relX * (tile.width  - 1));
            cRow = static_cast<int>(relY * (tile.height - 1));
        }
    }
    // 경계 클램프: 서브영역이 타일 밖으로 나가지 않도록
    cCol = std::max(halfPx, std::min(tile.width  - 1 - halfPx, cCol));
    cRow = std::max(halfPx, std::min(tile.height - 1 - halfPx, cRow));

    _meshCenterCol = cCol;
    _meshCenterRow = cRow;
    _meshHalfPx    = halfPx;

    _statusLabel->setText(QString("z=%1  수심 %2m ~ %3m  1km영역 중심(%4,%5) ±%6px")
                              .arg(tile.tileZ)
                              .arg(tile.minHeight(), 0, 'f', 0)
                              .arg(tile.maxHeight(), 0, 'f', 0)
                              .arg(cCol).arg(cRow).arg(halfPx));

    buildMesh(tile, cCol, cRow, halfPx);
    buildWaterPlane();
    buildCompass(tile, cCol, cRow, halfPx);
    updateVehicleMarker();
}

// ─────────────────────────────────────────────
// 높이맵 서브영역 → Qt3D 삼각형 메시
// cCol/cRow: 타일 내 중심 픽셀, halfPx: ±픽셀 반경 (= 1km / mpp)
// 항상 _worldHalfSize×2 유닛 크기로 스케일업하여 렌더링
// ─────────────────────────────────────────────
void TerrainWidget::buildMesh(const TerrainTile& tile, int cCol, int cRow, int halfPx)
{
    if (_meshEntity) {
        _meshEntity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _meshEntity;
        _meshEntity = nullptr;
    }

    const int   subW        = halfPx * 2 + 1;
    const int   subH        = halfPx * 2 + 1;
    const float scale       = _worldHalfSize / static_cast<float>(halfPx);
    const float heightScale = 0.05f;

    float minH = tile.minHeight();
    float maxH = tile.maxHeight();

    // ── 버텍스: position(3) + normal(3) + color(3) ──
    QByteArray vertexBytes(subW * subH * 9 * sizeof(float), Qt::Uninitialized);
    float* vp = reinterpret_cast<float*>(vertexBytes.data());

    for (int sr = 0; sr < subH; ++sr) {
        for (int sc = 0; sc < subW; ++sc) {
            float h = tile.heightAt(cCol - halfPx + sc, cRow - halfPx + sr);

            float xPos = (sc - halfPx) * scale;
            float zPos = (sr - halfPx) * scale;
            float yPos = h * heightScale;

            *vp++ = xPos; *vp++ = yPos; *vp++ = zPos;
            *vp++ = 0.0f; *vp++ = 1.0f; *vp++ = 0.0f;

            float r, g, b;
            if (h < 0.0f) {
                float depth = (-h) / (-minH + 1.0f);
                r = 0.0f;
                g = 0.2f * (1.0f - depth);
                b = 0.4f + 0.6f * (1.0f - depth);
            } else {
                float t = h / (maxH + 1.0f);
                r = 0.05f + t * 0.15f;
                g = 0.6f  - t * 0.25f;
                b = 0.05f + t * 0.05f;
            }
            *vp++ = r; *vp++ = g; *vp++ = b;
        }
    }

    // ── 인덱스 ──
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

    // ── Qt3D 지오메트리 조립 ──
    auto* geometry = new Qt3DRender::QGeometry();

    auto* vBuf = new Qt3DRender::QBuffer(geometry);
    vBuf->setData(vertexBytes);
    auto* iBuf = new Qt3DRender::QBuffer(geometry);
    iBuf->setData(indexBytes);

    auto makeAttr = [&](const QString& name, int offset, int size) {
        auto* a = new Qt3DRender::QAttribute(geometry);
        a->setName(name);
        a->setVertexBaseType(Qt3DRender::QAttribute::Float);
        a->setVertexSize(size);
        a->setByteOffset(offset * sizeof(float));
        a->setByteStride(9 * sizeof(float));
        a->setCount(static_cast<uint>(subW * subH));
        a->setBuffer(vBuf);
        geometry->addAttribute(a);
    };
    makeAttr(Qt3DRender::QAttribute::defaultPositionAttributeName(), 0, 3);
    makeAttr(Qt3DRender::QAttribute::defaultNormalAttributeName(),   3, 3);
    makeAttr(Qt3DRender::QAttribute::defaultColorAttributeName(),    6, 3);

    auto* idxAttr = new Qt3DRender::QAttribute(geometry);
    idxAttr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    idxAttr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    idxAttr->setCount(static_cast<uint>(quadCount * 6));
    idxAttr->setBuffer(iBuf);
    geometry->addAttribute(idxAttr);

    auto* renderer = new Qt3DRender::QGeometryRenderer();
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    auto* material = new Qt3DExtras::QPerVertexColorMaterial(_rootEntity);

    _meshEntity = new Qt3DCore::QEntity(_rootEntity);
    _meshEntity->addComponent(renderer);
    _meshEntity->addComponent(material);
}

// ─────────────────────────────────────────────
// 해수면 평면 생성
// 3m 기준(SEA_LEVEL_OFFSET * heightScale)에 반투명 파란 평면을 깔아서
// 육지가 평면 위로 솟아오르고 바다는 평면 아래로 가라앉아 해안선이 명확해짐
// ─────────────────────────────────────────────
void TerrainWidget::buildWaterPlane()
{
    if (_waterPlane) {
        _waterPlane->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _waterPlane;
        _waterPlane = nullptr;
    }

    _waterPlane = new Qt3DCore::QEntity(_rootEntity);

    auto* mesh = new Qt3DExtras::QPlaneMesh(_waterPlane);
    mesh->setWidth(_worldHalfSize * 2.0f);
    mesh->setHeight(_worldHalfSize * 2.0f);
    mesh->setMeshResolution(QSize(2, 2));

    // 반투명 파란 수면
    auto* mat = new Qt3DExtras::QPhongAlphaMaterial(_waterPlane);
    mat->setAmbient(QColor(0,  60, 120));
    mat->setDiffuse(QColor(0,  90, 180));
    mat->setSpecular(QColor(100, 180, 255));
    mat->setShininess(80.0f);
    mat->setAlpha(0.55f);  // 0=완전투명, 1=불투명 — 0.55면 아래 지형 살짝 보임

    auto* t = new Qt3DCore::QTransform();
    t->setTranslation(QVector3D(0.0f, 0.0f, 0.0f));

    _waterPlane->addComponent(mesh);
    _waterPlane->addComponent(mat);
    _waterPlane->addComponent(t);
}

// ─────────────────────────────────────────────
// 나침반 + 4개 모서리 좌표 레이블 생성
// 검증: 각 모서리에 실제 위경도를 표시하여 2D 지도와 대조 가능
// ─────────────────────────────────────────────
void TerrainWidget::buildCompass(const TerrainTile& tile, int cCol, int cRow, int halfPx)
{
    if (_compassEntity) {
        _compassEntity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _compassEntity;
        _compassEntity = nullptr;
    }

    _compassEntity = new Qt3DCore::QEntity(_rootEntity);

    const float hw     = _worldHalfSize;
    const float margin = hw + 18.0f;
    const float yText  = 5.0f;

    // ── 서브영역 4모서리의 실제 위경도 계산 ──────────
    const int    n    = 1 << tile.tileZ;
    const double lonW = static_cast<double>(tile.tileX)     / n * 360.0 - 180.0;
    const double lonE = static_cast<double>(tile.tileX + 1) / n * 360.0 - 180.0;
    const double latN = tileYToLat(tile.tileY,     n);
    const double latS = tileYToLat(tile.tileY + 1, n);

    double subLonW = lonW + (double)(cCol - halfPx) / tile.width  * (lonE - lonW);
    double subLonE = lonW + (double)(cCol + halfPx) / tile.width  * (lonE - lonW);
    double subLatN = latN + (double)(cRow - halfPx) / tile.height * (latS - latN);
    double subLatS = latN + (double)(cRow + halfPx) / tile.height * (latS - latN);

    auto addLabel = [&](const QString& text, float x, float y, float z3d,
                        float rotY, float w, const QColor& color)
    {
        auto* label = new Qt3DExtras::QText2DEntity(_compassEntity);
        label->setText(text);
        label->setFont(QFont("Arial", 8, QFont::Bold));
        label->setColor(color);
        label->setWidth(w);
        label->setHeight(14.0f);
        auto* t = new Qt3DCore::QTransform();
        t->setTranslation(QVector3D(x, y, z3d));
        t->setRotationY(rotY);
        label->addComponent(t);
    };

    addLabel("N", -7.0f,       yText, -margin,        0.0f, 14.0f, QColor(255, 80,  80));
    addLabel("S", -7.0f,       yText,  margin,       180.0f, 14.0f, QColor(200, 200, 200));
    addLabel("E",  margin,     yText, -7.0f,          -90.0f, 14.0f, QColor(200, 200, 200));
    addLabel("W", -margin-14,  yText, -7.0f,           90.0f, 14.0f, QColor(200, 200, 200));

    // 4모서리 실제 위경도 (서브영역 기준)
    addLabel(QString("%1°N\n%2°E").arg(subLatN,0,'f',4).arg(subLonW,0,'f',4),
             -hw-80, yText+10, -hw, 0.0f, 80.0f, QColor(255, 220, 100));
    addLabel(QString("%1°N\n%2°E").arg(subLatN,0,'f',4).arg(subLonE,0,'f',4),
              hw+5,  yText+10, -hw, 0.0f, 80.0f, QColor(255, 220, 100));
    addLabel(QString("%1°N\n%2°E").arg(subLatS,0,'f',4).arg(subLonW,0,'f',4),
             -hw-80, yText+10,  hw, 0.0f, 80.0f, QColor(180, 255, 180));
    addLabel(QString("%1°N\n%2°E").arg(subLatS,0,'f',4).arg(subLonE,0,'f',4),
              hw+5,  yText+10,  hw, 0.0f, 80.0f, QColor(180, 255, 180));
}

// ─────────────────────────────────────────────
// 차량 마커: 현재 위경도를 타일 내 3D 좌표로 변환
// ─────────────────────────────────────────────
void TerrainWidget::updateVehicleMarker()
{
    if (!_currentTile.isValid() || (_vehicleLat == 0.0 && _vehicleLon == 0.0)) return;

    // 마커 제거 후 재생성
    if (_vehicleMarker) {
        _vehicleMarker->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _vehicleMarker;
        _vehicleMarker = nullptr;
    }

    // 타일 내 픽셀 위치 계산 (정규화 0~1)
    // 타일 하나의 위도/경도 범위 계산
    const int z = _currentTile.tileZ;
    const int tx = _currentTile.tileX;
    const int ty = _currentTile.tileY;
    const int n = 1 << z;

    // 타일의 서쪽/동쪽 경도
    double lonWest  = static_cast<double>(tx)     / n * 360.0 - 180.0;
    double lonEast  = static_cast<double>(tx + 1) / n * 360.0 - 180.0;

    // 타일의 북쪽/남쪽 위도 (Web Mercator 역변환)
    double latNorth = tileYToLat(ty,     n);
    double latSouth = tileYToLat(ty + 1, n);

    // 타일 내 상대 위치 (0~1)
    double relX = (_vehicleLon - lonWest)  / (lonEast  - lonWest);
    double relY = (latNorth - _vehicleLat) / (latNorth - latSouth);  // row 방향 (위→아래=북→남)

    // 타일 범위 벗어나면 마커 표시 안 함
    if (relX < 0.0 || relX > 1.0 || relY < 0.0 || relY > 1.0) return;

    const float W = static_cast<float>(_currentTile.width);
    const float H = static_cast<float>(_currentTile.height);
    const float scale       = _worldHalfSize / static_cast<float>(_meshHalfPx);
    const float heightScale = 0.05f;

    int col = static_cast<int>(relX * (W - 1));
    int row = static_cast<int>(relY * (H - 1));

    // 서브영역 중심(_meshCenterCol, _meshCenterRow) 기준 좌표 변환
    float xPos = (col - _meshCenterCol) * scale;
    float zPos = (row - _meshCenterRow) * scale;
    float yPos = _currentTile.heightAt(col, row) * heightScale + 8.0f;

    // 빨간 구체 마커
    _vehicleMarker = new Qt3DCore::QEntity(_rootEntity);

    auto* sphere = new Qt3DExtras::QSphereMesh(_vehicleMarker);
    sphere->setRadius(5.0f);
    sphere->setRings(8);
    sphere->setSlices(8);

    auto* mat = new Qt3DExtras::QPhongMaterial(_vehicleMarker);
    mat->setAmbient(QColor(200, 20,  20));
    mat->setDiffuse(QColor(255, 60,  60));
    mat->setSpecular(QColor(255, 200, 200));
    mat->setShininess(60.0f);

    auto* t = new Qt3DCore::QTransform();
    t->setTranslation(QVector3D(xPos, yPos, zPos));

    _vehicleMarker->addComponent(sphere);
    _vehicleMarker->addComponent(mat);
    _vehicleMarker->addComponent(t);
}

// ─────────────────────────────────────────────
// 위경도 → 타일 인덱스 변환 (Web Mercator)
// ─────────────────────────────────────────────
int TerrainWidget::lonToTileX(double lon, int z)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << z)));
}

int TerrainWidget::latToTileY(double lat, int z)
{
    double latRad = lat * M_PI / 180.0;
    return static_cast<int>(
        std::floor((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * (1 << z)));
}

// tileY 인덱스 → 위도 역변환 (Web Mercator)
double TerrainWidget::tileYToLat(int ty, int n)
{
    double latRad = std::atan(std::sinh(M_PI * (1.0 - 2.0 * static_cast<double>(ty) / n)));
    return latRad * 180.0 / M_PI;
}
