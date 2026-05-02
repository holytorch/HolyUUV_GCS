#pragma once

#include <QString>
#include <QSerialPortInfo>

class UsbBoardInfo {
public:
    static UsbBoardInfo& instance();

    bool    isMavlinkBoard(const QSerialPortInfo& port) const;
    QString boardName(const QSerialPortInfo& port) const;

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

    QList<BoardEntry>   _boardInfo;
    QList<FallbackEntry> _descriptionFallback;
    QList<FallbackEntry> _manufacturerFallback;
};
