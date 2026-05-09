#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QHash>
#include "TileCache.h"

// ─────────────────────────────────────────────────────────────────────────────
// TileServer
// 127.0.0.1:17777 에서 동작하는 로컬 HTTP 타일 프록시 서버.
//
// 요청 경로 형식: /{source}/{z}/{x}/{y}.png
//   osm     → CartoDB dark_all  (3D 지형 텍스처 및 OSM 탭)
//   voyager → CartoDB Voyager   (수역 마스크 및 Voyager 탭)
//
// 처리 흐름:
//   HTTP GET 수신 → handleRequest() → TileCache.lookup()
//     캐시 히트: onTileFound()  → sendTile()  → 소켓 응답
//     캐시 미스: onTileMissed() → fetchAndCache() → CartoDB → store() → sendTile()
// ─────────────────────────────────────────────────────────────────────────────
class TileServer : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 PORT = 17777;

    explicit TileServer(TileCache* cache, QObject* parent = nullptr);

    bool start();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();

    void onTileFound(QPointer<QTcpSocket> socket, const QByteArray& data);
    void onTileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url);

private:
    void handleRequest(QTcpSocket* socket, const QString& path);
    void fetchAndCache(QPointer<QTcpSocket> socket, const QString& key, const QString& url);
    void sendTile(QTcpSocket* socket, const QByteArray& data);
    void sendNotFound(QTcpSocket* socket);

    QTcpServer             _server;
    TileCache*             _cache;
    QNetworkAccessManager  _nam;
    QHash<QTcpSocket*, QByteArray> _socketBuffers;
};
