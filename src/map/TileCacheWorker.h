#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QPointer>
#include <QTcpSocket>

// DB 작업 전담 워커 — moveToThread()로 별도 스레드에서 실행됨
// 메인 스레드와 신호/슬롯으로만 통신 (직접 함수 호출 금지)
class TileCacheWorker : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 MAX_BYTES   = 5LL * 1024 * 1024 * 1024;
    static constexpr qint64 EVICT_BYTES = 4LL * 1024 * 1024 * 1024;

    ~TileCacheWorker();

public slots:
    void init(const QString& dbPath);
    void lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void store(const QString& key, const QByteArray& data);

signals:
    void tileFound(QPointer<QTcpSocket> socket, const QByteArray& data);
    void tileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url);

private:
    void createTable();
    void evictIfNeeded();

    QSqlDatabase _db;
    QString      _connName{"holyuuv_tile_cache_worker"};
};
