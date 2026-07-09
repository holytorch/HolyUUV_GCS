#pragma once

#include <QObject>
#include <QThread>
#include <QPointer>
#include <QTcpSocket>

Q_DECLARE_METATYPE(QPointer<QTcpSocket>)

class TileCacheWorker;

// ─────────────────────────────────────────────────────────────────────────────
// TileCache
// The main-thread proxy for the tile cache. The actual DB work is performed by a
// TileCacheWorker running on a separate thread.
//
// Cache tiers:
//   Tier 1: SQLite (up to 5 GB, LRU auto-eviction) — managed by TileServer
//   Tier 2: CartoDB remote server                  — on a SQLite miss, downloaded then stored
//
// Usage flow:
//   TileServer.handleRequest()
//     → TileCache.lookup()               (main thread)
//       → TileCacheWorker.lookup()       (DB thread, QueuedConnection)
//         hit:  tileFound  signal → TileServer.onTileFound()  → socket response
//         miss: tileMissed signal → TileServer.onTileMissed() → CartoDB download
//   TileServer.fetchAndCache()
//     → TileCache.store()                (main thread)
//       → TileCacheWorker.store()        (DB thread)
// ─────────────────────────────────────────────────────────────────────────────
class TileCache : public QObject {
    Q_OBJECT
public:
    explicit TileCache(const QString& dbPath = QString(), QObject* parent = nullptr);
    ~TileCache();

    void lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void store(const QString& key, const QByteArray& data);

signals:
    void tileFound(QPointer<QTcpSocket> socket, const QByteArray& data);
    void tileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url);

    // Internal only: main → worker (QueuedConnection applied automatically)
    void _doInit(const QString& dbPath);
    void _doLookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void _doStore(const QString& key, const QByteArray& data);

private:
    QThread*         _thread;
    TileCacheWorker* _worker;
};
