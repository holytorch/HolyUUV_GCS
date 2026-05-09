#pragma once

#include <QObject>
#include <QThread>
#include <QPointer>
#include <QTcpSocket>

Q_DECLARE_METATYPE(QPointer<QTcpSocket>)

class TileCacheWorker;

// ─────────────────────────────────────────────────────────────────────────────
// TileCache
// 타일 캐시의 메인 스레드 프록시. 실제 DB 작업은 별도 스레드의 TileCacheWorker가 수행한다.
//
// 캐시 계층:
//   1순위: SQLite (최대 5 GB, LRU 자동 삭제)  — TileServer 관리
//   2순위: CartoDB 원격 서버                   — SQLite 미스 시 다운로드 후 저장
//
// 사용 흐름:
//   TileServer.handleRequest()
//     → TileCache.lookup()               (메인 스레드)
//       → TileCacheWorker.lookup()       (DB 스레드, QueuedConnection)
//         히트: tileFound  신호 → TileServer.onTileFound()  → 소켓 응답
//         미스: tileMissed 신호 → TileServer.onTileMissed() → CartoDB 다운로드
//   TileServer.fetchAndCache()
//     → TileCache.store()                (메인 스레드)
//       → TileCacheWorker.store()        (DB 스레드)
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

    // 내부 전용: 메인 → 워커 (QueuedConnection 자동 적용)
    void _doInit(const QString& dbPath);
    void _doLookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void _doStore(const QString& key, const QByteArray& data);

private:
    QThread*         _thread;
    TileCacheWorker* _worker;
};
