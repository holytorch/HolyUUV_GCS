#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QHash>
#include "TileCache.h"

// 로컬 HTTP 타일 프록시 (127.0.0.1:17777)
// Qt Location → TileServer → TileCache(DB 스레드) → CartoDB
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

    // TileCache 비동기 응답 수신
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
