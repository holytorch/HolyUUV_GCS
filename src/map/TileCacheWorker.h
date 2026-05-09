#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QPointer>
#include <QTcpSocket>

// ─────────────────────────────────────────────────────────────────────────────
// TileCacheWorker
// SQLite DB 작업을 전담하는 워커 객체. moveToThread()로 별도 스레드에서 실행된다.
// 메인 스레드와 시그널/슬롯으로만 통신하며, 직접 함수 호출은 금지된다.
//
// 캐시 정책:
//   - 최대 5 GB (MAX_BYTES) 초과 시 LRU 방식으로 4 GB (EVICT_BYTES)까지 삭제
//   - ts(타임스탬프) 컬럼을 접근 시마다 갱신해 자주 사용되는 타일을 보존
//   - WAL 모드 + NORMAL 동기화로 쓰기 성능과 안전성 균형
// ─────────────────────────────────────────────────────────────────────────────
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
