#include "TileCacheWorker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// ~TileCacheWorker()
// Closes the DB connection and removes the connection name from the Qt SQL driver.
// ─────────────────────────────────────────────────────────────────────────────
TileCacheWorker::~TileCacheWorker()
{
    if (_db.isOpen()) {
        _db.close();
        _db = QSqlDatabase();
        QSqlDatabase::removeDatabase(_connName);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// init()
// Runs on the worker thread. Ensures the DB file path exists, opens the SQLite
// connection, sets WAL mode, and creates the tiles table.
// Must be created inside this function to honor Qt's constraint that a
// QSqlDatabase connection belongs only to the thread that created it.
// ─────────────────────────────────────────────────────────────────────────────
void TileCacheWorker::init(const QString& dbPath)
{
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    _db = QSqlDatabase::addDatabase("QSQLITE", _connName);
    _db.setDatabaseName(dbPath);

    if (!_db.open()) {
        qCritical("TileCacheWorker: DB open failed: %s", qPrintable(_db.lastError().text()));
        return;
    }

    QSqlQuery pragma(_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA synchronous=NORMAL");

    createTable();
    qInfo("TileCacheWorker: opened %s (worker thread)", qPrintable(dbPath));
}


// ─────────────────────────────────────────────────────────────────────────────
// createTable()
// Creates the tiles table and the ts index (if absent). ts is used as the LRU
// eviction key.
// ─────────────────────────────────────────────────────────────────────────────
void TileCacheWorker::createTable()
{
    QSqlQuery q(_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS tiles (
            key  TEXT PRIMARY KEY,
            data BLOB NOT NULL,
            ts   INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_tiles_ts ON tiles(ts)");
}


// ─────────────────────────────────────────────────────────────────────────────
// lookup()
// Queries SQLite by key.
//   hit:  refreshes ts to the current time (LRU preservation) and emits tileFound.
//   miss: emits tileMissed so TileServer starts a remote download.
// ─────────────────────────────────────────────────────────────────────────────
void TileCacheWorker::lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url)
{
    QSqlQuery q(_db);
    q.prepare("SELECT data FROM tiles WHERE key = ?");
    q.addBindValue(key);

    if (q.exec() && q.next()) {
        QByteArray data = q.value(0).toByteArray();

        QSqlQuery upd(_db);
        upd.prepare("UPDATE tiles SET ts = strftime('%s','now') WHERE key = ?");
        upd.addBindValue(key);
        upd.exec();

        emit tileFound(socket, data);
        return;
    }

    emit tileMissed(socket, key, url);
}


// ─────────────────────────────────────────────────────────────────────────────
// store()
// Stores the tile data in SQLite (replacing an existing row) and calls
// evictIfNeeded() to keep the cache under its size limit.
// ─────────────────────────────────────────────────────────────────────────────
void TileCacheWorker::store(const QString& key, const QByteArray& data)
{
    QSqlQuery q(_db);
    q.prepare("INSERT OR REPLACE INTO tiles (key, data) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(data);

    if (!q.exec()) {
        qWarning("TileCacheWorker: store failed for %s: %s",
                 qPrintable(key), qPrintable(q.lastError().text()));
        return;
    }

    evictIfNeeded();
}


// ─────────────────────────────────────────────────────────────────────────────
// evictIfNeeded()
// If the total data size exceeds MAX_BYTES (5 GB), deletes tiles starting from the
// oldest ts until the cache is below EVICT_BYTES (4 GB).
// ─────────────────────────────────────────────────────────────────────────────
void TileCacheWorker::evictIfNeeded()
{
    QSqlQuery q(_db);
    q.exec("SELECT SUM(LENGTH(data)), COUNT(*) FROM tiles");
    if (!q.next()) return;

    qint64 total = q.value(0).toLongLong();
    qint64 count = q.value(1).toLongLong();

    if (total <= MAX_BYTES || count == 0) return;

    qint64 toFree      = total - EVICT_BYTES;
    double avgSize     = static_cast<double>(total) / count;
    int    deleteCount = static_cast<int>(toFree / avgSize) + 1;

    QSqlQuery del(_db);
    del.prepare("DELETE FROM tiles WHERE key IN "
                "(SELECT key FROM tiles ORDER BY ts ASC LIMIT ?)");
    del.addBindValue(deleteCount);
    del.exec();

    qInfo("TileCacheWorker: evicted %d tiles (%.1f MB freed, was %.1f MB)",
          deleteCount, toFree / 1024.0 / 1024.0, total / 1024.0 / 1024.0);
}
