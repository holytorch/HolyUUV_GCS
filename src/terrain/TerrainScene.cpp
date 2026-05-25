#include "TerrainScene.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QDir>
#include <QPainter>
#include <cmath>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseMapMaterial>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>

// TileServer가 중계하는 로컬 타일 URL 템플릿 (%1=z, %2=tx, %3=ty)
static const QString OSM_URL     = "http://localhost:17777/osm/%1/%2/%3.png";
static const QString VOYAGER_URL = "http://localhost:17777/voyager/%1/%2/%3.png";


double TerrainScene::metersPerPixel(double lat, int z)
{
    return 156543.03392 * std::cos(lat * M_PI / 180.0) / std::pow(2.0, z);
}

double TerrainScene::globalPixToLon(double gx, int z)
{
    return gx / (256.0 * (1 << z)) * 360.0 - 180.0;
}

double TerrainScene::globalPixToLat(double gy, int z)
{
    double n = static_cast<double>(1 << z);
    return std::atan(std::sinh(M_PI * (1.0 - 2.0 * gy / (n * 256.0)))) * 180.0 / M_PI;
}


TerrainScene::TerrainScene(QObject* parent)
    : QObject(parent)
{
    // Entity 트리는 attachTo() 시점에 외부 parent의 자식으로 생성한다.
    // 그래야 parent destroy 시 자동 정리되고, _rootEntity QPointer가 null로 바뀐다.
    connect(&_nam, &QNetworkAccessManager::finished,
            this, &TerrainScene::_onReplyFinished);
}

TerrainScene::~TerrainScene() = default;


// 외부 parent(Qt3DWindow root 또는 QML Scene3D Entity)에 entity 트리를 생성한다.
// 호출 시점에 mesh 데이터가 캐시되어 있으면 즉시 rebuild (네트워크 재fetch 없음).
// 캐시 없으면 _startFetch()를 발동 (첫 attachTo 케이스).
//
// _rootEntity는 외부 parent 소유 — parent destroy 시 자동 정리되고
// QPointer 덕에 dangling 없이 null이 된다. 다음 attachTo 호출에서 다시 빌드됨.
void TerrainScene::attachTo(Qt3DCore::QEntity* parent)
{
    if (!parent) return;

    // 이전 _rootEntity가 살아 있으면(즉, 이전 parent가 아직 살아 있으면)
    // 명시적으로 deleteLater() — 새 parent에서 새 entity 트리를 다시 빌드.
    if (_rootEntity)
        _rootEntity->deleteLater();

    _rootEntity = new Qt3DCore::QEntity(parent);
    _meshEntity = nullptr;
    _vehicleMarker = nullptr;
    _vehicleXform = nullptr;

    if (_currentTile.isValid()) {
        // 캐시된 높이맵/텍스처로 즉시 시각화 복원 (네트워크 무관)
        _buildMesh();
        _updateVehicleMarker();
        _resetCameraToTerrain();
    }
    // attachTo는 entity 트리 부착만 책임. fetch는 호출자(showEvent/QML)가
    // TileServer가 listening 시작한 후 명시적으로 loadTile()로 트리거.
}


void TerrainScene::loadTile(double lat, double lon, int zoom)
{
    _lat  = lat;
    _lon  = lon;
    _zoom = zoom;
    _startFetch();
}


void TerrainScene::updateVehiclePosition(double lat, double lon)
{
    if (lat == 0.0 && lon == 0.0) return;

    _vehicleLat = lat;
    _vehicleLon = lon;

    if (!_firstFixReceived) {
        _firstFixReceived = true;
        // entity 트리가 살아 있을 때만 즉시 fetch.
        // 아직 attachTo 전이면 lat/lon만 저장 — 다음 attachTo가 이 좌표로 fetch 시작.
        if (_rootEntity)
            loadTile(lat, lon, _zoom);
    } else if (_rootEntity && _meshEntity) {
        _updateVehicleMarker();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// _startFetch()
// 중심 위경도를 Web Mercator 픽셀로 변환하고 필요한 타일 범위를 동시에 요청.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::_startFetch()
{
    double mpp    = metersPerPixel(_lat, _zoom);
    int    halfPx = std::max(2, static_cast<int>(std::ceil(500.0 / mpp)));

    double fracX = (_lon + 180.0) / 360.0 * (1 << _zoom);
    double latR  = _lat * M_PI / 180.0;
    double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                   / 2.0 * (1 << _zoom);
    int gCX = static_cast<int>(fracX * 256);
    int gCY = static_cast<int>(fracY * 256);

    _osmTX0 = (gCX - halfPx) / 256;
    _osmTY0 = (gCY - halfPx) / 256;
    int tx1 = (gCX + halfPx) / 256;
    int ty1 = (gCY + halfPx) / 256;
    _osmTNX = tx1 - _osmTX0 + 1;
    _osmTNY = ty1 - _osmTY0 + 1;

    _stitchCX  = gCX - _osmTX0 * 256;
    _stitchCY  = gCY - _osmTY0 * 256;
    _subHalfPx = halfPx;

    _osmImages.clear();
    _osmPending = _osmTNX * _osmTNY;

    _voyagerImages.clear();
    _voyagerPending = _osmTNX * _osmTNY;

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


void TerrainScene::_onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->property("positronTX").isValid())
        _handleVoyagerTile(reply);
    else
        _handleOsmTile(reply);
}


void TerrainScene::_handleOsmTile(QNetworkReply* reply)
{
    int otx = reply->property("osmTX").toInt();
    int oty = reply->property("osmTY").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        if (img.loadFromData(reply->readAll()) && !img.isNull()) {
            _osmImages[qMakePair(otx, oty)] =
                img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
        } else {
            qWarning("OSM tile (%d,%d): 이미지 디코딩 실패", otx, oty);
        }
    } else {
        qWarning("OSM tile (%d,%d) error: %s", otx, oty,
                 qPrintable(reply->errorString()));
    }

    if (--_osmPending <= 0 && _voyagerPending <= 0)
        _stitchAndBuild();
}


void TerrainScene::_handleVoyagerTile(QNetworkReply* reply)
{
    int otx = reply->property("positronTX").toInt();
    int oty = reply->property("positronTY").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        if (img.loadFromData(reply->readAll()) && !img.isNull()) {
            _voyagerImages[qMakePair(otx, oty)] =
                img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
        } else {
            qWarning("Voyager tile (%d,%d): 이미지 디코딩 실패", otx, oty);
        }
    } else {
        qWarning("Voyager tile (%d,%d) error: %s", otx, oty,
                 qPrintable(reply->errorString()));
    }

    if (--_voyagerPending <= 0 && _osmPending <= 0)
        _stitchAndBuild();
}


// ─────────────────────────────────────────────────────────────────────────────
// _stitchAndBuild()
// 모든 타일이 수신되면: 스티칭 → 수역 마스크 → 블러 → 텍스처 저장 → buildMesh
// 자세한 알고리즘 설명은 TerrainWidget.cpp의 동일 함수 주석 참고.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::_stitchAndBuild()
{
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

    // 수역 판별
    for (int sy = 0; sy < subH; ++sy) {
        for (int sx = 0; sx < subW; ++sx) {
            int  px  = qBound(0, _stitchCX - _subHalfPx + sx, lightStitched.width()  - 1);
            int  py  = qBound(0, _stitchCY - _subHalfPx + sy, lightStitched.height() - 1);
            QRgb col = lightStitched.pixel(px, py);
            int  r = qRed(col), g = qGreen(col), b = qBlue(col);
            bool water = (b > r + 15) && (b > 150) && (g > 150);
            _currentTile.heights[sy * subW + sx] = water ? SEA_DEPTH : LAND_H;
        }
    }

    // 분리형 박스 블러 3회로 해안선 계단 완화
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

    // 텍스처 저장
    static int seq = 0;
    ++seq;
    _darkTexPath = QString("%1/holyuuv_dark_%2.png").arg(QDir::tempPath()).arg(seq);
    darkStitched.save(_darkTexPath);
    _osmTexPath = _darkTexPath;

    // entity 트리가 살아 있을 때만 시각화 빌드.
    // null이면 _currentTile만 캐시 — 다음 attachTo가 즉시 빌드함.
    if (_rootEntity) {
        _buildMesh();
        _updateVehicleMarker();
        _resetCameraToTerrain();
    }

    emit terrainReady();
}


// ─────────────────────────────────────────────────────────────────────────────
// _buildMesh()
// _currentTile 높이맵 + _osmTexPath 텍스처로 Qt3D 메시 entity 생성.
// 자세한 정점 포맷 설명은 TerrainWidget.cpp 참고.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::_buildMesh()
{
    if (!_rootEntity) return;

    if (_meshEntity) {
        _meshEntity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete _meshEntity;
        _meshEntity = nullptr;
    }

    const TerrainTile& tile = _currentTile;
    const int   subW  = tile.width;
    const int   subH  = tile.height;
    const float scale = _worldHalfSize / static_cast<float>(_subHalfPx);

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
            *vp++ = h * _heightScale;
            *vp++ = (sr - _subHalfPx) * scale;

            int sc0 = std::max(0, sc - 1), sc1 = std::min(subW - 1, sc + 1);
            int sr0 = std::max(0, sr - 1), sr1 = std::min(subH - 1, sr + 1);
            float dx   = (sc1 - sc0) * scale;
            float dz   = (sr1 - sr0) * scale;
            float dy_x = (tile.heightAt(sc1, sr) - tile.heightAt(sc0, sr)) * _heightScale;
            float dy_z = (tile.heightAt(sc, sr1) - tile.heightAt(sc, sr0)) * _heightScale;
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

    auto* tex = new Qt3DRender::QTexture2D(_rootEntity.data());
    tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    auto* texImg = new Qt3DRender::QTextureImage(tex);
    texImg->setSource(QUrl::fromLocalFile(_osmTexPath));
    texImg->setMirrored(false);
    tex->addTextureImage(texImg);

    auto* mat = new Qt3DExtras::QDiffuseMapMaterial(_rootEntity.data());
    mat->setDiffuse(tex);
    mat->setAmbient(QColor(180, 180, 180));
    mat->setSpecular(QColor(20, 20, 20));
    mat->setShininess(5.0f);

    auto* meshEntity = new Qt3DCore::QEntity(_rootEntity.data());
    meshEntity->addComponent(renderer);
    meshEntity->addComponent(mat);
    _meshEntity = meshEntity;
}


TerrainScene::WorldPos TerrainScene::_latLonToWorld(double lat, double lon) const
{
    WorldPos out;

    const double fracX = (lon + 180.0) / 360.0 * (1 << _zoom);
    const double latR  = lat * M_PI / 180.0;
    const double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                         / 2.0 * (1 << _zoom);
    const double relX  = (fracX * 256 - _osmTX0 * 256) - _stitchCX;
    const double relY  = (fracY * 256 - _osmTY0 * 256) - _stitchCY;

    out.inBounds = (std::abs(relX) <= _subHalfPx && std::abs(relY) <= _subHalfPx);
    if (!out.inBounds) return out;

    const float scale = _worldHalfSize / static_cast<float>(_subHalfPx);
    const int   sc    = static_cast<int>(relX + _subHalfPx);
    const int   sr    = static_cast<int>(relY + _subHalfPx);

    out.x        = static_cast<float>(relX) * scale;
    out.z        = static_cast<float>(relY) * scale;
    out.terrainH = _currentTile.heightAt(sc, sr);
    return out;
}


void TerrainScene::_updateVehicleMarker()
{
    if (!_rootEntity) return;
    if (_vehicleLat == 0.0 && _vehicleLon == 0.0) return;
    if (!_currentTile.isValid()) return;

    const WorldPos p = _latLonToWorld(_vehicleLat, _vehicleLon);

    const float yPos = p.inBounds
        ? std::max(p.terrainH * _heightScale, 0.0f) + 5.0f
        : 0.0f;

    if (!_vehicleMarker) {
        auto* marker = new Qt3DCore::QEntity(_rootEntity.data());

        auto* sphere = new Qt3DExtras::QSphereMesh(marker);
        sphere->setRadius(5.0f);
        sphere->setRings(12);
        sphere->setSlices(12);

        auto* mat = new Qt3DExtras::QPhongMaterial(marker);
        mat->setAmbient(QColor(200, 20, 20));
        mat->setDiffuse(QColor(255, 60, 60));
        mat->setSpecular(QColor(255, 200, 200));
        mat->setShininess(60.0f);

        auto* xform = new Qt3DCore::QTransform();

        marker->addComponent(sphere);
        marker->addComponent(mat);
        marker->addComponent(xform);

        _vehicleMarker = marker;
        _vehicleXform  = xform;
    }

    _vehicleXform->setTranslation(QVector3D(p.x, yPos, p.z));
    _vehicleMarker->setEnabled(p.inBounds);
}


void TerrainScene::_resetCameraToTerrain()
{
    if (!_camera) return;

    QVector3D target(0.0f, 0.0f, 0.0f);
    if (_vehicleMarker && _currentTile.isValid()) {
        const WorldPos p = _latLonToWorld(_vehicleLat, _vehicleLon);
        if (p.inBounds)
            target = QVector3D(p.x, p.terrainH * _heightScale, p.z);
    }

    float mpp           = static_cast<float>(metersPerPixel(_lat, _zoom));
    float unitsPerMeter = _worldHalfSize / (_subHalfPx * mpp);

    float up    = 700.0f * unitsPerMeter;
    float south = 500.0f * unitsPerMeter;
    _camera->setPosition(target + QVector3D(0.0f, up, south));
    _camera->setViewCenter(target);
    _camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
}
