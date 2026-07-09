#include "TileServer.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// Tile URL templates
// %1 = zoom, %2 = tileX, %3 = tileY
// ─────────────────────────────────────────────────────────────────────────────
static const QString CARTO_URL_TPL =
    "https://a.basemaps.cartocdn.com/dark_all/%1/%2/%3.png";

static const QString VOYAGER_URL_TPL =
    "https://a.basemaps.cartocdn.com/rastertiles/voyager/%1/%2/%3.png";


// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// Connects TileCache's tileFound / tileMissed signals to this server's slots.
// tileFound  → SQLite hit, respond immediately
// tileMissed → SQLite miss, download from the remote server then respond
// ─────────────────────────────────────────────────────────────────────────────
TileServer::TileServer(TileCache* cache, QObject* parent)
    : QObject(parent)
    , _cache(cache)
{
    connect(_cache, &TileCache::tileFound,  this, &TileServer::onTileFound);
    connect(_cache, &TileCache::tileMissed, this, &TileServer::onTileMissed);
    qInfo("[init] TileServer");
}


// ─────────────────────────────────────────────────────────────────────────────
// ~TileServer()
// ─────────────────────────────────────────────────────────────────────────────
TileServer::~TileServer()
{
    qInfo("[exit] TileServer");
}


// ─────────────────────────────────────────────────────────────────────────────
// start()
// Starts listening for TCP on 127.0.0.1:17777.
// Qt Location (the QML map) and the TerrainWidget request tiles from this port.
// ─────────────────────────────────────────────────────────────────────────────
bool TileServer::start()
{
    if (!_server.listen(QHostAddress::LocalHost, PORT)) {
        qCritical("TileServer: listen failed: %s", qPrintable(_server.errorString()));
        return false;
    }
    connect(&_server, &QTcpServer::newConnection, this, &TileServer::onNewConnection);
    qInfo("TileServer: listening on 127.0.0.1:%d", PORT);
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// onNewConnection()
// Called whenever a new TCP connection arrives.
// Initializes a receive buffer per socket and connects its readyRead / disconnected
// signals. Several requests may arrive at once, so a while loop handles them all.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onNewConnection()
{
    while (_server.hasPendingConnections()) {
        QTcpSocket* socket = _server.nextPendingConnection();
        _socketBuffers[socket] = QByteArray();

        connect(socket, &QTcpSocket::readyRead,    this, &TileServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TileServer::onSocketDisconnected);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// onReadyRead()
// Called whenever data arrives on a socket.
// Because TCP may not deliver the whole request at once, bytes are accumulated in a
// buffer; once the end of the HTTP header (\r\n\r\n) is detected, the request's
// first line is parsed and passed to handleRequest(). Non-GET methods get a 404.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    _socketBuffers[socket] += socket->readAll();

    const QByteArray& buf = _socketBuffers[socket];
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    QString firstLine = QString::fromLatin1(buf.left(buf.indexOf("\r\n")));
    QStringList parts = firstLine.split(' ');
    if (parts.size() < 2 || parts[0] != "GET") {
        sendNotFound(socket);
        _socketBuffers.remove(socket);
        return;
    }

    QString path = parts[1];
    _socketBuffers[socket].clear();

    handleRequest(socket, path);
}


// ─────────────────────────────────────────────────────────────────────────────
// onSocketDisconnected()
// When a client disconnects, removes that socket's receive buffer and deletes it.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onSocketDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    _socketBuffers.remove(socket);
    socket->deleteLater();
}


// ─────────────────────────────────────────────────────────────────────────────
// handleRequest()
// Parses the URL path to extract the tile source (osm / voyager) and z/x/y
// coordinates. Determines the SQLite cache key and remote URL per source, then
// requests an asynchronous lookup from TileCache.
//
// Path format: /{source}/{z}/{x}/{y}.png
//   osm     → CartoDB dark_all  (3D terrain texture and OSM tab)
//   voyager → CartoDB Voyager   (water-mask generation and Voyager tab)
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::handleRequest(QTcpSocket* socket, const QString& path)
{
    QStringList segs = path.split('/', Qt::SkipEmptyParts);
    if (segs.size() < 4) {
        sendNotFound(socket);
        return;
    }

    QString source = segs[0];
    QString yStr   = segs.last();
    yStr.remove(".png").remove(".jpg");

    QString z = segs[segs.size() - 3];
    QString x = segs[segs.size() - 2];
    QString y = yStr;

    QString key, url;
    if (source == "voyager") {
        key = QString("voyager/%1/%2/%3").arg(z, x, y);
        url = VOYAGER_URL_TPL.arg(z, x, y);
    } else {
        key = QString("carto_dark/%1/%2/%3").arg(z, x, y);
        url = CARTO_URL_TPL.arg(z, x, y);
    }

    _cache->lookup(key, QPointer<QTcpSocket>(socket), url);
}


// ─────────────────────────────────────────────────────────────────────────────
// onTileFound()
// Called when TileCache finds the tile in SQLite.
// If the socket is still connected, responds with the tile data immediately.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onTileFound(QPointer<QTcpSocket> socket, const QByteArray& data)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
        sendTile(socket, data);
}


// ─────────────────────────────────────────────────────────────────────────────
// onTileMissed()
// Called when TileCache does not find the tile in SQLite.
// Starts a download from the remote server via fetchAndCache().
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onTileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url)
{
    fetchAndCache(socket, key, url);
}


// ─────────────────────────────────────────────────────────────────────────────
// fetchAndCache()
// Sends an HTTP GET to the remote tile server (asynchronously).
// On response, stores it in SQLite (so later requests hit the cache) and responds
// to the client socket. Uses QPointer so it is safe even if the socket closes
// before the response arrives.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::fetchAndCache(QPointer<QTcpSocket> socket, const QString& key, const QString& url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "HolyUUV_GCS/1.0");

    QNetworkReply* reply = _nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, socket, key]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning("TileServer: fetch failed [%s]: %s",
                     qPrintable(key), qPrintable(reply->errorString()));
            if (socket && socket->state() == QAbstractSocket::ConnectedState)
                sendNotFound(socket);
            return;
        }

        QByteArray data = reply->readAll();
        _cache->store(key, data);

        if (socket && socket->state() == QAbstractSocket::ConnectedState)
            sendTile(socket, data);
    });
}


// ─────────────────────────────────────────────────────────────────────────────
// sendTile()
// Assembles an HTTP 200 response and writes it to the socket.
// Closes the connection afterward (Connection: close).
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::sendTile(QTcpSocket* socket, const QByteArray& data)
{
    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: image/png\r\n";
    response += "Content-Length: " + QByteArray::number(data.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += data;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}


// ─────────────────────────────────────────────────────────────────────────────
// sendNotFound()
// Sends an HTTP 404 response.
// Called when the request path is invalid or the remote download failed.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::sendNotFound(QTcpSocket* socket)
{
    QByteArray response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
