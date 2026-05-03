#include "TileCacheWorker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

TileCacheWorker::~TileCacheWorker()
{
    if (_db.isOpen()) {
        _db.close();
        _db = QSqlDatabase();
        QSqlDatabase::removeDatabase(_connName);
    }
}

// DB 초기화 — 워커 스레드에서 실행되므로 connection이 이 스레드에 귀속됨
void TileCacheWorker::init(const QString& dbPath)
{
    // 데이터베이스 파일이 저장될 디렉토리가 없으면 생성
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    // SQLite 드라이버를 로드하고 연결 이름을 설정
    // "QSQLITE"는 별도의 서버 없이 로컬 파일로 작동하는 DB 엔진
    _db = QSqlDatabase::addDatabase("QSQLITE", _connName);
    _db.setDatabaseName(dbPath);

    if (!_db.open()) {
        qCritical("TileCacheWorker: DB open failed: %s", qPrintable(_db.lastError().text()));
        return;
    }

    // WAL 모드로 쓰기 성능 향상, 동시 읽기 허용
    QSqlQuery pragma(_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA synchronous=NORMAL");

    // 테이블 생성
    createTable();
    qInfo("TileCacheWorker: opened %s (worker thread)", qPrintable(dbPath));
}

void TileCacheWorker::createTable()
{
    QSqlQuery q(_db);
    // tiles 테이블: key(타일 식별자), data(타일 이미지), ts(마지막 접근 시각, LRU 삭제용)
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS tiles (
            key  TEXT PRIMARY KEY,
            data BLOB NOT NULL,
            ts   INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        )
    )");
    // ts 인덱스: LRU 삭제 시 ORDER BY ts가 빠르게 동작하도록
    q.exec("CREATE INDEX IF NOT EXISTS idx_tiles_ts ON tiles(ts)");
}

// Qt 파일캐시 미스 시 TileServer가 호출
// SQLite에서 타일 조회 → 히트: tileFound 신호 / 미스: tileMissed 신호 → CartoDB 다운로드로 넘어감
void TileCacheWorker::lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url)
{
    QSqlQuery q(_db);
    q.prepare("SELECT data FROM tiles WHERE key = ?");
    q.addBindValue(key);

    if (q.exec() && q.next()) {
        QByteArray data = q.value(0).toByteArray();

        // LRU: 조회할 때마다 ts 갱신 → 자주 쓰는 타일은 삭제 대상에서 밀려남
        QSqlQuery upd(_db);
        upd.prepare("UPDATE tiles SET ts = strftime('%s','now') WHERE key = ?");
        upd.addBindValue(key);
        upd.exec();

        emit tileFound(socket, data);
        return;
    }

    emit tileMissed(socket, key, url);
}

// CartoDB에서 다운로드한 타일을 SQLite에 저장 (put() 호출 후 자동으로 5GB 초과 시 LRU 삭제)
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

// put() 호출 후 자동 실행 → 5GB 초과 시 ts 오래된 타일부터 삭제해서 4GB로 줄임 (LRU)
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
