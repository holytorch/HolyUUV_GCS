#include "TileCache.h"
#include "TileCacheWorker.h"
#include <QStandardPaths>
#include <QDebug>

TileCache::TileCache(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , _thread(new QThread(this))
    , _worker(new TileCacheWorker)
{
    // QPointer<QTcpSocket>를 스레드간 신호 파라미터로 쓰기 위해 등록
    qRegisterMetaType<QPointer<QTcpSocket>>("QPointer<QTcpSocket>");

    //dbPath이 비어있으면 기본 캐시 위치에 tiles/tiles.db로 설정(~/.cache/HolyUUV_GCS/tiles/tiles.db)
    QString path = dbPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tiles/tiles.db"
        : dbPath;

    _worker->moveToThread(_thread);

    // 메인 → 워커 (QueuedConnection: 워커 스레드의 이벤트 루프에서 실행됨)
    connect(this, &TileCache::_doInit,   _worker, &TileCacheWorker::init);
    connect(this, &TileCache::_doLookup, _worker, &TileCacheWorker::lookup);
    connect(this, &TileCache::_doStore,  _worker, &TileCacheWorker::store);

    // 워커 → 메인 (결과 중계)
    connect(_worker, &TileCacheWorker::tileFound,  this, &TileCache::tileFound);
    connect(_worker, &TileCacheWorker::tileMissed, this, &TileCache::tileMissed);

    _thread->start();
    emit _doInit(path);  // DB 초기화를 워커 스레드에서 실행
}

TileCache::~TileCache()
{
    _thread->quit();
    _thread->wait();  // 워커 스레드 종료 대기
    delete _worker;
}

void TileCache::lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url)
{
    emit _doLookup(key, socket, url);
}

void TileCache::store(const QString& key, const QByteArray& data)
{
    emit _doStore(key, data);
}
