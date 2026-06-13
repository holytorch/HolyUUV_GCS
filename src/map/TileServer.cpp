#include "TileServer.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// 타일 URL 템플릿
// %1 = zoom, %2 = tileX, %3 = tileY
// ─────────────────────────────────────────────────────────────────────────────
static const QString CARTO_URL_TPL =
    "https://a.basemaps.cartocdn.com/dark_all/%1/%2/%3.png";

static const QString VOYAGER_URL_TPL =
    "https://a.basemaps.cartocdn.com/rastertiles/voyager/%1/%2/%3.png";


// ─────────────────────────────────────────────────────────────────────────────
// 생성자
// TileCache의 tileFound / tileMissed 신호를 이 서버의 슬롯에 연결한다.
// tileFound  → SQLite 히트, 바로 응답
// tileMissed → SQLite 미스, 원격 서버에서 다운로드 후 응답
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
// 127.0.0.1:17777 에서 TCP 리슨을 시작한다.
// Qt Location(QML 맵)과 TerrainWidget이 이 포트로 타일을 요청한다.
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
// 새 TCP 연결이 들어올 때마다 호출된다.
// 소켓마다 수신 버퍼를 초기화하고 readyRead / disconnected 시그널을 연결한다.
// 동시 요청이 여러 개일 수 있으므로 while 로 한 번에 처리한다.
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
// 소켓에 데이터가 도착할 때마다 호출된다.
// TCP 특성상 한 번에 전체가 오지 않을 수 있으므로 버퍼에 누적한 뒤
// HTTP 헤더 끝(\r\n\r\n)이 감지되면 요청 첫 줄을 파싱해 handleRequest()로 넘긴다.
// GET 이외의 메서드는 404로 응답한다.
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
// 클라이언트가 연결을 끊으면 해당 소켓의 수신 버퍼를 제거하고 소켓을 삭제한다.
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
// URL 경로를 파싱해 타일 소스(osm / voyager)와 z/x/y 좌표를 추출한다.
// 소스에 따라 SQLite 캐시 키와 원격 URL을 결정한 뒤 TileCache에 비동기 조회를 요청한다.
//
// 경로 형식: /{source}/{z}/{x}/{y}.png
//   osm     → CartoDB dark_all  (3D 지형 텍스처 및 OSM 탭 표시용)
//   voyager → CartoDB Voyager   (수역 마스크 생성 및 Voyager 탭 표시용)
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
// TileCache가 SQLite에서 타일을 찾았을 때 호출된다.
// 소켓이 아직 연결 중이면 즉시 타일 데이터를 응답한다.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onTileFound(QPointer<QTcpSocket> socket, const QByteArray& data)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
        sendTile(socket, data);
}


// ─────────────────────────────────────────────────────────────────────────────
// onTileMissed()
// TileCache가 SQLite에서 타일을 찾지 못했을 때 호출된다.
// fetchAndCache()로 원격 서버에서 다운로드를 시작한다.
// ─────────────────────────────────────────────────────────────────────────────
void TileServer::onTileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url)
{
    fetchAndCache(socket, key, url);
}


// ─────────────────────────────────────────────────────────────────────────────
// fetchAndCache()
// 원격 타일 서버에 HTTP GET 요청을 보낸다(비동기).
// 응답이 오면 SQLite에 저장(이후 요청은 캐시 히트)하고 클라이언트 소켓에 응답한다.
// QPointer를 사용해 응답 도착 전에 소켓이 닫혀도 안전하게 처리한다.
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
// HTTP 200 응답을 조립해 소켓으로 전송한다.
// 전송 후 연결을 끊는다(Connection: close).
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
// HTTP 404 응답을 전송한다.
// 요청 경로가 잘못됐거나 원격 다운로드에 실패했을 때 호출된다.
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
