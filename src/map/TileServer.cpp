#include "TileServer.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

static const QString TILE_URL_TPL =
    "https://a.basemaps.cartocdn.com/dark_all/%1/%2/%3.png";

// TileCache 비동기 응답(tileFound/tileMissed)을 이 TileServer 슬롯으로 연결
TileServer::TileServer(TileCache* cache, QObject* parent)
    : QObject(parent)
    , _cache(cache)
{
    connect(_cache, &TileCache::tileFound,  this, &TileServer::onTileFound);
    connect(_cache, &TileCache::tileMissed, this, &TileServer::onTileMissed);
}

// 127.0.0.1:17777 에서 HTTP 서버 시작
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

// 새 클라이언트 연결 처리 (Qt Location이 타일 요청할 때마다 호출)
void TileServer::onNewConnection()
{
    // 동시에 여러 연결이 올 수 있어서 while로 전부 처리
    while (_server.hasPendingConnections()) {
        QTcpSocket* socket = _server.nextPendingConnection();
        _socketBuffers[socket] = QByteArray();  // 소켓별 HTTP 수신 버퍼 초기화

        connect(socket, &QTcpSocket::readyRead,    this, &TileServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TileServer::onSocketDisconnected);
    }
}

// 소켓에서 데이터 수신 시 호출 → HTTP 요청 파싱
void TileServer::onReadyRead()
{
    // readyRead 신호를 보낸 소켓이 누구인지 확인 (소켓이 여러 개라 구분 필요)
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // TCP는 데이터가 여러 패킷으로 나뉘어 올 수 있어서 버퍼에 누적
    _socketBuffers[socket] += socket->readAll();

    const QByteArray& buf = _socketBuffers[socket];
    // \r\n\r\n = HTTP 헤더 끝, 없으면 아직 요청이 완전히 안 온 것 → 더 기다림
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    // 첫 줄 파싱: "GET /10/886/410.png HTTP/1.1" → ["GET", "/10/886/410.png", "HTTP/1.1"]
    QString firstLine = QString::fromLatin1(buf.left(buf.indexOf("\r\n")));
    QStringList parts = firstLine.split(' ');
    if (parts.size() < 2 || parts[0] != "GET") {
        sendNotFound(socket);
        _socketBuffers.remove(socket);
        return;
    }

    QString path = parts[1];  // "/10/886/410.png"
    _socketBuffers[socket].clear();

    handleRequest(socket, path);
}

// 연결 해제 시 버퍼 및 소켓 정리
void TileServer::onSocketDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    _socketBuffers.remove(socket);
    socket->deleteLater();
}

// path에서 z/x/y 파싱 → SQLite 비동기 조회 요청
void TileServer::handleRequest(QTcpSocket* socket, const QString& path)
{
    QStringList segs = path.split('/', Qt::SkipEmptyParts);
    if (segs.size() < 3) {
        sendNotFound(socket);
        return;
    }

    QString yStr = segs.last();
    yStr.remove(".png").remove(".jpg");

    QString z = segs[segs.size() - 3];
    QString x = segs[segs.size() - 2];
    QString y = yStr;

    QString key = QString("carto_dark/%1/%2/%3").arg(z, x, y);
    QString url = TILE_URL_TPL.arg(z, x, y);

    // 비동기 DB 조회 → onTileFound 또는 onTileMissed 콜백
    _cache->lookup(key, QPointer<QTcpSocket>(socket), url);
}

// DB 히트: SQLite에서 꺼낸 타일 즉시 응답
void TileServer::onTileFound(QPointer<QTcpSocket> socket, const QByteArray& data)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
        sendTile(socket, data);
}

// DB 미스: CartoDB에서 다운로드 후 SQLite 저장
void TileServer::onTileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url)
{
    fetchAndCache(socket, key, url);
}

// CartoDB HTTP 요청 (비동기) → 완료 시 SQLite 저장 + Qt Location에 응답
void TileServer::fetchAndCache(QPointer<QTcpSocket> socket, const QString& key, const QString& url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "HolyUUV_GCS/1.0");

    QNetworkReply* reply = _nam.get(req);  // 논블로킹, 완료되면 finished 신호

    // QPointer: 소켓이 먼저 삭제돼도 null 체크로 안전하게 처리
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
        _cache->store(key, data);  // 워커 스레드에서 비동기 저장

        if (socket && socket->state() == QAbstractSocket::ConnectedState)
            sendTile(socket, data);
    });
}

// HTTP 200 응답 조립 후 전송
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

// HTTP 404 응답 전송
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
