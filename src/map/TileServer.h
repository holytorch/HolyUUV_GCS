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
// A local HTTP tile-proxy server running on 127.0.0.1:17777.
//
// Request path format: /{source}/{z}/{x}/{y}.png
//   osm     → CartoDB dark_all  (3D terrain texture and the OSM tab)
//   voyager → CartoDB Voyager   (water mask and the Voyager tab)
//
// Processing flow:
//   HTTP GET received → handleRequest() → TileCache.lookup()
//     cache hit:  onTileFound()  → sendTile()  → socket response
//     cache miss: onTileMissed() → fetchAndCache() → CartoDB → store() → sendTile()
// ─────────────────────────────────────────────────────────────────────────────
class TileServer : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 PORT = 17777;

    explicit TileServer(TileCache* cache, QObject* parent = nullptr);
    ~TileServer() override;

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
