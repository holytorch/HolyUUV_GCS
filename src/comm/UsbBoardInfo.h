#pragma once

#include <QString>
#include <QSerialPortInfo>

// ─────────────────────────────────────────────────────────────────────────────
// UsbBoardInfo
// 리소스에 내장된 USBBoardInfo.json을 읽어 MAVLink 보드를 식별하는 싱글턴.
//
// 감지 우선순위:
//   1. VID/PID 직접 매칭
//   2. 포트 설명(description) 정규식 매칭
//   3. 제조사(manufacturer) 정규식 매칭
//
// isMavlinkBoard()가 true를 반환하면 Application은 해당 포트로 SerialLink를 생성한다.
// ─────────────────────────────────────────────────────────────────────────────
class UsbBoardInfo {
public:
    static UsbBoardInfo& instance();

    bool    isMavlinkBoard(const QSerialPortInfo& port) const;
    QString boardName(const QSerialPortInfo& port)      const;

private:
    struct BoardEntry {
        quint16 vendorID;
        quint16 productID;
        QString boardClass;
        QString name;
    };

    struct FallbackEntry {
        QString regExp;
        QString boardClass;
    };

    UsbBoardInfo();
    void _loadJson();

    QList<BoardEntry>    _boardInfo;
    QList<FallbackEntry> _descriptionFallback;
    QList<FallbackEntry> _manufacturerFallback;
};
