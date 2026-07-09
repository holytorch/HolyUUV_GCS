#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QPointer>
#include <QTcpSocket>

// ─────────────────────────────────────────────────────────────────────────────
// TileCacheWorker
// A worker object dedicated to SQLite DB operations, run on a separate thread via
// moveToThread(). It communicates with the main thread only through signals/slots;
// direct function calls are forbidden.
//
// Cache policy:
//   - when the size exceeds 5 GB (MAX_BYTES), evict LRU-style down to 4 GB (EVICT_BYTES)
//   - refresh the ts (timestamp) column on every access to preserve frequently used tiles
//   - WAL mode + NORMAL synchronization to balance write performance and safety
// ─────────────────────────────────────────────────────────────────────────────
class TileCacheWorker : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 MAX_BYTES   = 5LL * 1024 * 1024 * 1024;
    static constexpr qint64 EVICT_BYTES = 4LL * 1024 * 1024 * 1024;

    ~TileCacheWorker();

public slots:
    void init(const QString& dbPath);
    void lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void store(const QString& key, const QByteArray& data);

signals:
    void tileFound(QPointer<QTcpSocket> socket, const QByteArray& data);
    void tileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url);

private:
    void createTable();
    void evictIfNeeded();

    QSqlDatabase _db;
    QString      _connName{"holyuuv_tile_cache_worker"};
};
