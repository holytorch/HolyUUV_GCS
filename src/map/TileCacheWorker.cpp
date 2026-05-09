#include "TileCacheWorker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// ~TileCacheWorker()
// DB 연결을 닫고 Qt SQL 드라이버에서 연결 이름을 제거한다.
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
// 워커 스레드에서 실행된다. DB 파일 경로를 보장하고 SQLite 연결을 열고
// WAL 모드를 설정한 뒤 tiles 테이블을 생성한다.
// QSqlDatabase 연결은 생성한 스레드에만 귀속된다는 Qt 제약을 준수하기 위해
// 반드시 이 함수 안에서 생성해야 한다.
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
// tiles 테이블과 ts 인덱스를 생성한다 (없으면). ts는 LRU 삭제 기준으로 사용된다.
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
// key로 SQLite를 조회한다.
//   히트: ts를 현재 시각으로 갱신(LRU 보존)하고 tileFound 신호를 발신한다.
//   미스: tileMissed 신호를 발신해 TileServer가 원격 다운로드를 시작하게 한다.
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
// 타일 데이터를 SQLite에 저장(존재하면 교체)하고 evictIfNeeded()를 호출해
// 캐시 크기를 제한 이하로 유지한다.
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
// 전체 데이터 크기가 MAX_BYTES(5 GB)를 초과하면 ts가 가장 오래된 타일부터
// 삭제해 EVICT_BYTES(4 GB) 이하로 줄인다.
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
