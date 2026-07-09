#pragma once

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QPair>
#include <QImage>
#include <QString>
#include <climits>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

namespace Qt3DRender { class QCamera; }

// ─────────────────────────────────────────────────────────────────────────────
// TerrainScene
// A Minecraft-chunk-style 3D terrain scene. Each tile (chunk) has its own mesh
// entity; as the robot moves, only the chunks within the render radius
// (kRenderDist) are dynamically loaded, and chunks that fall outside are unloaded.
// All chunks are placed in a fixed global coordinate system (relative to
// _originPx), so the terrain stays put even as the robot moves.
//
// Load order: the robot's own chunk → the forward (heading) direction → the rest by
// distance.
//
// Usage:
//   scene->setCamera(cam);
//   scene->attachTo(rootEntity);
//   scene->loadTile(lat, lon, zoom);          // pin the origin + load the first chunk
//   scene->updateVehiclePosition(lat, lon);   // move → refresh marker + chunks
//   scene->updateVehicleDepth(depthMeters);   // depth → marker y
// ─────────────────────────────────────────────────────────────────────────────
class TerrainScene : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt3DCore::QEntity* rootEntity READ rootEntity CONSTANT)
public:
    explicit TerrainScene(QObject* parent = nullptr);
    ~TerrainScene() override;

    Qt3DCore::QEntity* rootEntity() const { return _rootEntity.data(); }
    int                currentZoom() const { return _zoom; }
    Q_INVOKABLE bool   hasTerrainData() const { return !_chunks.isEmpty(); }

    Q_INVOKABLE void setCamera(Qt3DRender::QCamera* cam) { _camera = cam; }
    Q_INVOKABLE void attachTo(Qt3DCore::QEntity* parent);

    Q_INVOKABLE void loadTile(double lat, double lon, int zoom = 17);

    // Multi-robot: a marker per sysid. Called from C++ (MainWindow) on vehicle
    // telemetry updates.
    void updateVehiclePosition(int sysid, double lat, double lon);
    void updateVehicleDepth(int sysid, double depthMeters);   // depth below surface (m, positive)
    void setVehicleYaw(int sysid, double deg);                // marker cone direction (deg, 0=N)
    void removeVehicle(int sysid);                            // remove a vehicle marker

    // Set which active vehicle the chunk loading / camera follows + its heading
    // (used for forward-priority loading).
    void setActiveSysid(int sysid);
    void setVehicleHeading(double deg) { _vehicleHeading = deg; }

signals:
    void terrainReady();

private slots:
    void _onReplyFinished(QNetworkReply* reply);

private:
    static double metersPerPixel(double lat, int z);
    // lat/lon → global Web Mercator pixel (at zoom z)
    static void   latLonToGlobalPx(double lat, double lon, int z, double& gx, double& gy);

    void _updateChunks();                 // load/unload based on the render radius
    void _requestChunk(int tx, int ty);   // request the OSM + Voyager tiles
    void _buildChunkMesh(int tx, int ty); // build the chunk mesh from received tiles
    void _updateMarker(int sysid);        // create/position/rotate the sysid marker
    void _resetCameraToTerrain();
    bool _activePos(double& lat, double& lon) const;  // active vehicle position (if any)

    // global pixel → world coordinate
    float _worldX(double globalPx) const { return static_cast<float>(globalPx - _originPxX) * kWorldPerPixel; }
    float _worldZ(double globalPy) const { return static_cast<float>(globalPy - _originPxY) * kWorldPerPixel; }

    QPointer<Qt3DCore::QEntity>    _rootEntity;

    // Multi-robot: a marker per sysid (cone entity + transform + latest pose).
    // parent=_rootEntity (RAII)
    struct VehicleMarker {
        QPointer<Qt3DCore::QEntity>    entity;
        QPointer<Qt3DCore::QTransform> xform;
        double lat = 0.0, lon = 0.0, depth = 0.0, yaw = 0.0;
        bool   hasPos = false;
    };
    QMap<int, VehicleMarker> _markers;
    int    _activeSysid = 0;        // the vehicle the chunk loading / camera follows

    Qt3DRender::QCamera* _camera = nullptr;
    QNetworkAccessManager _nam;

    double _lat  = 37.52951029463262;
    double _lon  = 126.94149832867085;
    int    _zoom = 17;

    double _vehicleHeading = 0.0;   // active vehicle heading (deg, 0=N) — for forward-priority chunk loading
    bool   _firstFixReceived = false;

    // ── Chunk system ─────────────────────────────────────────────────────────
    struct Chunk {
        QPointer<Qt3DCore::QEntity> entity;
        QImage osm;        // OSM tile for the texture
        QImage voyager;    // water-mask tile
        bool   osmReady     = false;
        bool   voyagerReady = false;
        bool   built        = false;
    };
    QMap<QPair<int,int>, Chunk> _chunks;   // (tileX, tileY) → chunk

    bool _originSet = false;
    int  _originPxX = 0, _originPxY = 0;       // global coordinate origin (pixels)
    int  _curTileX  = INT_MIN, _curTileY = INT_MIN;  // the tile the robot was last in

    static constexpr float kWorldPerPixel = 0.25f;   // world units per tile pixel (tile = 64 units)
    static constexpr float _heightScale   = 0.05f;   // height scale factor
    static constexpr int   kRenderDist    = 2;       // chunk render radius R → (2R+1)² grid
    static constexpr int   kChunkQuads    = 32;      // grid quads per chunk
};
