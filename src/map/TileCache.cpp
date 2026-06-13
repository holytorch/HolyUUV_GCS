#include "TileCache.h"
#include "TileCacheWorker.h"
#include <QStandardPaths>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// TileCache()
// TileCacheWorker를 별도 스레드로 이동시키고 메인↔워커 신호 연결을 설정한다.
// dbPath가 비어 있으면 기본 캐시 경로를 사용한다 (~/.cache/HolyUUV_GCS/tiles/tiles.db).
// DB 초기화는 _doInit 신호를 통해 워커 스레드에서 비동기로 수행된다.
// ─────────────────────────────────────────────────────────────────────────────
TileCache::TileCache(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , _thread(new QThread(this))
    , _worker(new TileCacheWorker)
{
    qRegisterMetaType<QPointer<QTcpSocket>>("QPointer<QTcpSocket>");

    QString path = dbPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tiles/tiles.db"
        : dbPath;

    _worker->moveToThread(_thread);

    connect(this, &TileCache::_doInit,   _worker, &TileCacheWorker::init);
    connect(this, &TileCache::_doLookup, _worker, &TileCacheWorker::lookup);
    connect(this, &TileCache::_doStore,  _worker, &TileCacheWorker::store);

    connect(_worker, &TileCacheWorker::tileFound,  this, &TileCache::tileFound);
    connect(_worker, &TileCacheWorker::tileMissed, this, &TileCache::tileMissed);

    qInfo("[init] TileCache");
    _thread->start();
    qInfo("[init] TileCache.worker (thread)");
    emit _doInit(path);
}


// ─────────────────────────────────────────────────────────────────────────────
// ~TileCache()
// 워커 스레드의 이벤트 루프를 종료하고 완전히 멈출 때까지 대기한 뒤 워커를 삭제한다.
// ─────────────────────────────────────────────────────────────────────────────
TileCache::~TileCache()
{
    _thread->quit();
    _thread->wait();
    qInfo("[exit] TileCache.worker (thread)");
    delete _worker;
    qInfo("[exit] TileCache");
}


// ─────────────────────────────────────────────────────────────────────────────
// lookup()
// _doLookup 신호를 통해 워커 스레드에 비동기 조회를 요청한다.
// 결과는 tileFound 또는 tileMissed 신호로 수신된다.
// ─────────────────────────────────────────────────────────────────────────────
void TileCache::lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url)
{
    emit _doLookup(key, socket, url);
}


// ─────────────────────────────────────────────────────────────────────────────
// store()
// _doStore 신호를 통해 워커 스레드에 비동기 저장을 요청한다.
// ─────────────────────────────────────────────────────────────────────────────
void TileCache::store(const QString& key, const QByteArray& data)
{
    emit _doStore(key, data);
}
