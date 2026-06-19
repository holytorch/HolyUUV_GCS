#include "TerrainScene.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QDir>
#include <cmath>
#include <vector>
#include <algorithm>

#include <QQuaternion>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QConeMesh>
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

// 수역 마스크 → 높이값
static constexpr float SEA_DEPTH = -1000.0f;
static constexpr float LAND_H    =     1.0f;


double TerrainScene::metersPerPixel(double lat, int z)
{
    return 156543.03392 * std::cos(lat * M_PI / 180.0) / std::pow(2.0, z);
}

void TerrainScene::latLonToGlobalPx(double lat, double lon, int z, double& gx, double& gy)
{
    const double n     = static_cast<double>(1 << z);
    const double fracX = (lon + 180.0) / 360.0 * n;
    const double latR  = lat * M_PI / 180.0;
    const double fracY = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI)
                         / 2.0 * n;
    gx = fracX * 256.0;
    gy = fracY * 256.0;
}


TerrainScene::TerrainScene(QObject* parent)
    : QObject(parent)
{
    connect(&_nam, &QNetworkAccessManager::finished,
            this, &TerrainScene::_onReplyFinished);
    qInfo("[init] TerrainScene");
}

TerrainScene::~TerrainScene()
{
    qInfo("[exit] TerrainScene");
}


// ─────────────────────────────────────────────────────────────────────────────
// attachTo()
// 외부 parent(QML Scene3D Entity)에 _rootEntity를 새로 만든다.
// 이미 받아둔 청크 이미지가 있으면 네트워크 없이 메시를 재빌드한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::attachTo(Qt3DCore::QEntity* parent)
{
    if (!parent) return;

    if (_rootEntity)
        _rootEntity->deleteLater();

    _rootEntity = new Qt3DCore::QEntity(parent);

    // 새 root → 기존 마커 엔티티 무효화 (포즈 데이터는 _markers에 유지 → 재빌드)
    for (auto& m : _markers) { m.entity = nullptr; m.xform = nullptr; }

    if (!_chunks.isEmpty()) {
        // 캐시된 타일 이미지로 즉시 메시 재빌드
        for (auto it = _chunks.begin(); it != _chunks.end(); ++it) {
            it->entity = nullptr;
            it->built  = false;
            if (it->osmReady && it->voyagerReady)
                _buildChunkMesh(it.key().first, it.key().second);
        }
        for (auto it = _markers.begin(); it != _markers.end(); ++it)
            if (it->hasPos) _updateMarker(it.key());
        _resetCameraToTerrain();
    } else {
        double lat, lon;
        if (_activePos(lat, lon))
            loadTile(lat, lon, _zoom);
    }
    // 둘 다 아니면(no cache, no GPS) QML 호출자가 loadTile로 트리거.
}


// ─────────────────────────────────────────────────────────────────────────────
// loadTile()
// 전역 좌표 원점을 (최초 1회) 고정하고, 현재 위치 주변 청크 로드를 시작한다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::loadTile(double lat, double lon, int zoom)
{
    _lat  = lat;
    _lon  = lon;
    _zoom = zoom;

    if (!_originSet) {
        double gx, gy;
        latLonToGlobalPx(lat, lon, zoom, gx, gy);
        _originPxX = static_cast<int>(gx);
        _originPxY = static_cast<int>(gy);
        _originSet = true;
    }
    _updateChunks();
}


// 활성 차량의 위치를 반환 (마커 포즈가 있을 때만 true). 청크/카메라 추종에 사용.
bool TerrainScene::_activePos(double& lat, double& lon) const
{
    auto it = _markers.find(_activeSysid);
    if (it != _markers.end() && it->hasPos) {
        lat = it->lat;
        lon = it->lon;
        return true;
    }
    return false;
}


void TerrainScene::updateVehiclePosition(int sysid, double lat, double lon)
{
    if (lat == 0.0 && lon == 0.0) return;

    VehicleMarker& m = _markers[sysid];
    m.lat = lat; m.lon = lon; m.hasPos = true;

    // 첫 위치 fix → 전역 원점 고정 (모든 마커가 이 기준으로 배치). 기본 좌표로 열린
    // 청크가 있으면 폐기 후 이 위치 기준으로 재로드 (화면 밖 이탈 방지).
    if (!_firstFixReceived) {
        _firstFixReceived = true;
        if (_rootEntity) {
            for (auto& ch : _chunks)
                if (ch.entity) ch.entity->deleteLater();
            _chunks.clear();
            _originSet = false;
            _curTileX  = INT_MIN;
            _curTileY  = INT_MIN;
            loadTile(lat, lon, _zoom);
        }
    }

    if (!_rootEntity || !_originSet) return;

    _updateMarker(sysid);

    // 활성 차량이면 청크/카메라가 따라간다 (새 타일 진입 시 갱신)
    if (sysid == _activeSysid) {
        double gx, gy;
        latLonToGlobalPx(lat, lon, _zoom, gx, gy);
        const int tx = static_cast<int>(std::floor(gx / 256.0));
        const int ty = static_cast<int>(std::floor(gy / 256.0));
        if (tx != _curTileX || ty != _curTileY)
            _updateChunks();
    }
}


void TerrainScene::updateVehicleDepth(int sysid, double depthMeters)
{
    VehicleMarker& m = _markers[sysid];
    m.depth = (depthMeters > 0.0) ? depthMeters : 0.0;
    if (_rootEntity)
        _updateMarker(sysid);
}


void TerrainScene::setVehicleYaw(int sysid, double deg)
{
    VehicleMarker& m = _markers[sysid];
    m.yaw = deg;
    if (_rootEntity)
        _updateMarker(sysid);
}


void TerrainScene::removeVehicle(int sysid)
{
    auto it = _markers.find(sysid);
    if (it == _markers.end()) return;
    if (it->entity)
        it->entity->deleteLater();
    _markers.erase(it);
}


// 청크 로딩/카메라가 추종할 활성 차량 지정. 새 활성 위치로 재중심한다.
void TerrainScene::setActiveSysid(int sysid)
{
    _activeSysid = sysid;
    if (_rootEntity && _originSet) {
        _updateChunks();
        _resetCameraToTerrain();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// _updateChunks()
// 중심(로봇) 타일 기준 렌더 반경 안의 청크를 로드하고, 밖의 청크를 언로드한다.
// 로드 순서: 중심 → 전방(heading) → 거리순.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::_updateChunks()
{
    if (!_originSet || !_rootEntity) return;

    // 중심 = 활성 차량 위치(있으면), 없으면 마지막 loadTile 좌표
    double clat = _lat, clon = _lon;
    _activePos(clat, clon);

    double gx, gy;
    latLonToGlobalPx(clat, clon, _zoom, gx, gy);
    const int ctx = static_cast<int>(std::floor(gx / 256.0));
    const int cty = static_cast<int>(std::floor(gy / 256.0));
    _curTileX = ctx;
    _curTileY = cty;

    const int R = kRenderDist;

    // 1) 언로드 — 렌더 반경 밖 청크 제거
    for (auto it = _chunks.begin(); it != _chunks.end(); ) {
        const int tx = it.key().first;
        const int ty = it.key().second;
        if (std::abs(tx - ctx) > R || std::abs(ty - cty) > R) {
            if (it->entity)
                it->entity->deleteLater();
            it = _chunks.erase(it);
        } else {
            ++it;
        }
    }

    // 2) 로드 — 반경 안의 미보유 청크를 우선순위 정렬해 요청
    const double hdg = _vehicleHeading * M_PI / 180.0;
    const double fx  = std::sin(hdg);   // 그리드: +x=동, +y=남 (heading 0=N → -y)
    const double fy  = -std::cos(hdg);

    struct Req { int tx, ty; double score; };
    std::vector<Req> todo;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            const int tx = ctx + dx;
            const int ty = cty + dy;
            if (_chunks.contains(qMakePair(tx, ty))) continue;
            const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));
            double align = 0.0;
            if (dist > 1e-6) align = (dx * fx + dy * fy) / dist;   // 전방 정렬도 [-1,1]
            // 점수 낮을수록 먼저: 중심(거리0) → 전방 → 측면 → 후방, 그다음 먼 거리
            todo.push_back({tx, ty, dist - 0.5 * align});
        }
    }
    std::sort(todo.begin(), todo.end(),
              [](const Req& a, const Req& b) { return a.score < b.score; });

    for (const auto& r : todo)
        _requestChunk(r.tx, r.ty);
}


void TerrainScene::_requestChunk(int tx, int ty)
{
    // placeholder 항목 생성 (중복 요청 방지 — _updateChunks의 contains 체크가 본다)
    Chunk& ch = _chunks[qMakePair(tx, ty)];
    ch.osmReady = ch.voyagerReady = ch.built = false;

    auto* osmRep = _nam.get(QNetworkRequest(QUrl(OSM_URL.arg(_zoom).arg(tx).arg(ty))));
    osmRep->setProperty("ctx",  tx);
    osmRep->setProperty("cty",  ty);
    osmRep->setProperty("kind", QStringLiteral("osm"));

    auto* voyRep = _nam.get(QNetworkRequest(QUrl(VOYAGER_URL.arg(_zoom).arg(tx).arg(ty))));
    voyRep->setProperty("ctx",  tx);
    voyRep->setProperty("cty",  ty);
    voyRep->setProperty("kind", QStringLiteral("voyager"));
}


void TerrainScene::_onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    const int     tx   = reply->property("ctx").toInt();
    const int     ty   = reply->property("cty").toInt();
    const QString kind = reply->property("kind").toString();

    auto key = qMakePair(tx, ty);
    auto it  = _chunks.find(key);
    if (it == _chunks.end()) return;   // 이미 언로드된 청크의 늦은 응답 — 무시
    Chunk& ch = it.value();

    if (reply->error() == QNetworkReply::NoError) {
        QImage img;
        if (img.loadFromData(reply->readAll()) && !img.isNull()) {
            QImage scaled = img.scaled(256, 256).convertToFormat(QImage::Format_RGB32);
            if (kind == QLatin1String("voyager")) {
                ch.voyager      = scaled;
                ch.voyagerReady = true;
            } else {
                ch.osm      = scaled;
                ch.osmReady = true;
            }
        } else {
            qWarning("Chunk (%d,%d) %s: 이미지 디코딩 실패", tx, ty, qPrintable(kind));
        }
    } else {
        qWarning("Chunk (%d,%d) %s error: %s", tx, ty, qPrintable(kind),
                 qPrintable(reply->errorString()));
    }

    if (ch.osmReady && ch.voyagerReady && !ch.built && _rootEntity)
        _buildChunkMesh(tx, ty);
}


// ─────────────────────────────────────────────────────────────────────────────
// _buildChunkMesh()
// 한 타일(청크)의 높이맵 + 텍스처로 Qt3D 메시 엔티티를 만든다.
// 정점은 전역 좌표계에 배치되어 인접 청크와 자연히 이어진다.
// ─────────────────────────────────────────────────────────────────────────────
void TerrainScene::_buildChunkMesh(int tx, int ty)
{
    if (!_rootEntity) return;
    auto it = _chunks.find(qMakePair(tx, ty));
    if (it == _chunks.end()) return;
    Chunk& ch = it.value();
    if (ch.osm.isNull() || ch.voyager.isNull()) return;

    if (ch.entity) {
        ch.entity->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        delete ch.entity;
        ch.entity = nullptr;
    }

    const int N = kChunkQuads;   // quad 수
    const int V = N + 1;         // 한 변 정점 수

    const double tilePxX = static_cast<double>(tx) * 256.0;
    const double tilePxY = static_cast<double>(ty) * 256.0;

    // 수역 마스크 → 높이 (타일 픽셀 0..255)
    auto heightAt = [&](int px, int py) -> float {
        px = qBound(0, px, 255);
        py = qBound(0, py, 255);
        const QRgb c = ch.voyager.pixel(px, py);
        const int  r = qRed(c), g = qGreen(c), b = qBlue(c);
        const bool water = (b > r + 15) && (b > 150) && (g > 150);
        return water ? SEA_DEPTH : LAND_H;
    };

    QByteArray vertexBytes(V * V * 8 * sizeof(float), Qt::Uninitialized);
    float* vp = reinterpret_cast<float*>(vertexBytes.data());

    for (int j = 0; j < V; ++j) {
        const int py = (j * 255) / N;
        for (int i = 0; i < V; ++i) {
            const int px = (i * 255) / N;
            const float h = heightAt(px, py);

            *vp++ = _worldX(tilePxX + px);
            *vp++ = h * _heightScale;
            *vp++ = _worldZ(tilePxY + py);

            // 노멀 (중앙 차분)
            const float hl = heightAt(px - 1, py), hr = heightAt(px + 1, py);
            const float hd = heightAt(px, py - 1), hu = heightAt(px, py + 1);
            float nx = (hl - hr) * _heightScale;
            float nz = (hd - hu) * _heightScale;
            float ny = 2.0f * kWorldPerPixel;
            const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nl > 1e-6f) { nx /= nl; ny /= nl; nz /= nl; }
            else            { nx = 0.f; ny = 1.f; nz = 0.f; }
            *vp++ = nx; *vp++ = ny; *vp++ = nz;

            *vp++ = static_cast<float>(px) / 255.0f;   // u
            *vp++ = static_cast<float>(py) / 255.0f;   // v
        }
    }

    QByteArray indexBytes(N * N * 6 * sizeof(uint32_t), Qt::Uninitialized);
    uint32_t* ip = reinterpret_cast<uint32_t*>(indexBytes.data());
    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            const uint32_t tl = static_cast<uint32_t>(j * V + i);
            const uint32_t tr = tl + 1;
            const uint32_t bl = tl + static_cast<uint32_t>(V);
            const uint32_t br = bl + 1;
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
        a->setCount(static_cast<uint>(V * V));
        a->setBuffer(vBuf);
        geometry->addAttribute(a);
    };
    addAttr(Qt3DRender::QAttribute::defaultPositionAttributeName(),          0, 3);
    addAttr(Qt3DRender::QAttribute::defaultNormalAttributeName(),            3, 3);
    addAttr(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(), 6, 2);

    auto* idxAttr = new Qt3DRender::QAttribute(geometry);
    idxAttr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    idxAttr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    idxAttr->setCount(static_cast<uint>(N * N * 6));
    idxAttr->setBuffer(iBuf);
    geometry->addAttribute(idxAttr);

    auto* renderer = new Qt3DRender::QGeometryRenderer();
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    // 텍스처 — OSM 타일을 임시 PNG로 저장 후 로드
    static int seq = 0;
    ++seq;
    const QString texPath = QString("%1/holyuuv_chunk_%2_%3_%4.png")
                                .arg(QDir::tempPath()).arg(tx).arg(ty).arg(seq);
    ch.osm.save(texPath);

    auto* tex = new Qt3DRender::QTexture2D(_rootEntity.data());
    tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    auto* texImg = new Qt3DRender::QTextureImage(tex);
    texImg->setSource(QUrl::fromLocalFile(texPath));
    texImg->setMirrored(false);
    tex->addTextureImage(texImg);

    auto* mat = new Qt3DExtras::QDiffuseMapMaterial(_rootEntity.data());
    mat->setDiffuse(tex);
    mat->setAmbient(QColor(180, 180, 180));
    mat->setSpecular(QColor(20, 20, 20));
    mat->setShininess(5.0f);

    auto* ent = new Qt3DCore::QEntity(_rootEntity.data());
    ent->addComponent(renderer);
    ent->addComponent(mat);

    ch.entity = ent;
    ch.built  = true;

    emit terrainReady();
}


// sysid 마커(콘) 생성/위치/회전 갱신. 위치 fix가 있어야 렌더된다.
void TerrainScene::_updateMarker(int sysid)
{
    if (!_rootEntity || !_originSet) return;
    auto it = _markers.find(sysid);
    if (it == _markers.end() || !it->hasPos) return;
    VehicleMarker& m = it.value();

    double gx, gy;
    latLonToGlobalPx(m.lat, m.lon, _zoom, gx, gy);
    const float x = _worldX(gx);
    const float z = _worldZ(gy);

    // 해수면(y=0) 기준 실제 수심만큼 마커를 내린다. 수평과 동일 스케일 → 비율 정확.
    const float mpp           = static_cast<float>(metersPerPixel(m.lat, _zoom));
    const float unitsPerMeter = (mpp > 1e-9f) ? kWorldPerPixel / mpp : 0.0f;
    const float y             = -static_cast<float>(m.depth) * unitsPerMeter;

    if (!m.entity) {
        auto* marker = new Qt3DCore::QEntity(_rootEntity.data());

        // 방향성 콘(화살표) — 뱃머리 방향을 가리킨다. 기본 축은 +Y(뾰족한 끝).
        auto* cone = new Qt3DExtras::QConeMesh(marker);
        cone->setTopRadius(0.0f);
        cone->setBottomRadius(2.5f);
        cone->setLength(9.0f);
        cone->setRings(2);
        cone->setSlices(16);

        auto* mat = new Qt3DExtras::QPhongMaterial(marker);
        mat->setAmbient(QColor(200, 20, 20));
        mat->setDiffuse(QColor(255, 60, 60));
        mat->setSpecular(QColor(255, 200, 200));
        mat->setShininess(60.0f);

        auto* xform = new Qt3DCore::QTransform();

        marker->addComponent(cone);
        marker->addComponent(mat);
        marker->addComponent(xform);

        m.entity = marker;
        m.xform  = xform;
    }

    // 콘을 수평으로 눕혀(tip→북) yaw만큼 회전 → 끝이 뱃머리(compass) 방향을 가리킴.
    //   Rx(-90): +Y(끝) → -Z(북),  Ry(-yaw): 북 기준 시계방향으로 yaw만큼 스윙
    const QQuaternion rot =
        QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -static_cast<float>(m.yaw))
      * QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -90.0f);
    m.xform->setRotation(rot);
    m.xform->setTranslation(QVector3D(x, y, z));
    m.entity->setEnabled(true);
}


void TerrainScene::_resetCameraToTerrain()
{
    if (!_camera) return;

    QVector3D target(0.0f, 0.0f, 0.0f);
    double lat, lon;
    if (_originSet && _activePos(lat, lon)) {
        double gx, gy;
        latLonToGlobalPx(lat, lon, _zoom, gx, gy);
        target = QVector3D(_worldX(gx), 0.0f, _worldZ(gy));
    }

    const float mpp           = static_cast<float>(metersPerPixel(_lat, _zoom));
    const float unitsPerMeter = (mpp > 1e-9f) ? kWorldPerPixel / mpp : 1.0f;

    const float up    = 700.0f * unitsPerMeter;
    const float south = 500.0f * unitsPerMeter;
    _camera->setPosition(target + QVector3D(0.0f, up, south));
    _camera->setViewCenter(target);
    _camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
}
