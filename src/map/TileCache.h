#pragma once

#include <QObject>
#include <QThread>
#include <QPointer>
#include <QTcpSocket>

Q_DECLARE_METATYPE(QPointer<QTcpSocket>)

class TileCacheWorker;

// ── 타일 캐시 구조 ──────────────────────────────────────────
// 1순위: Qt 파일캐시 (50MB) - Qt Location 내장, 히트시 TileServer 요청 없음
// 2순위: SQLite (5GB)       - TileServer가 관리, 오프라인 장기 보관
// 3순위: CartoDB (인터넷)   - SQLite 미스시 다운로드 후 SQLite에 저장
//
// Qt 파일캐시 미스 → TileServer(127.0.0.1:17777) → SQLite 조회
//   히트: SQLite에서 반환 (인터넷 요청 없음)
//   미스: CartoDB 다운로드 → SQLite 저장 → 반환
// ────────────────────────────────────────────────────────────

// 메인 스레드에서 사용하는 프록시 — 실제 DB 작업은 TileCacheWorker(별도 스레드)가 수행
class TileCache : public QObject {
    Q_OBJECT
public:
    explicit TileCache(const QString& dbPath = QString(), QObject* parent = nullptr);
    ~TileCache();

    // 비동기 조회: 결과는 tileFound / tileMissed 신호로 수신
    void lookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);

    // 비동기 저장
    void store(const QString& key, const QByteArray& data);

signals:
    void tileFound(QPointer<QTcpSocket> socket, const QByteArray& data);
    void tileMissed(QPointer<QTcpSocket> socket, const QString& key, const QString& url);

    // 내부용: 메인 스레드 → 워커 스레드 (QueuedConnection 자동 적용)
    void _doInit(const QString& dbPath);
    void _doLookup(const QString& key, QPointer<QTcpSocket> socket, const QString& url);
    void _doStore(const QString& key, const QByteArray& data);

private:
    QThread*         _thread;
    TileCacheWorker* _worker;
};
