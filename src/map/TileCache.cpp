#include "TileCache.h"
#include "TileCacheWorker.h"
#include <QStandardPaths>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// TileCache()
// Moves the TileCacheWorker onto a separate thread and wires the main↔worker
// signal connections. If dbPath is empty, the default cache path is used
// (~/.cache/HolyUUV_GCS/tiles/tiles.db). DB initialization runs asynchronously on
// the worker thread via the _doInit signal.
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
// Stops the worker thread's event loop, waits for it to fully halt, then deletes
// the worker.
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
// Requests an asynchronous lookup on the worker thread via the _doLookup signal.
// The result arrives as a tileFound or tileMissed signal.
// ─────────────────────────────────────────────────────────────────────────────
void TileCache::lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url)
{
    emit _doLookup(key, socket, url);
}


// ─────────────────────────────────────────────────────────────────────────────
// store()
// Requests an asynchronous store on the worker thread via the _doStore signal.
// ─────────────────────────────────────────────────────────────────────────────
void TileCache::store(const QString& key, const QByteArray& data)
{
    emit _doStore(key, data);
}
