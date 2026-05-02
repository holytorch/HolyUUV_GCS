#include "UsbBoardInfo.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

UsbBoardInfo& UsbBoardInfo::instance()
{
    static UsbBoardInfo inst;
    return inst;
}

UsbBoardInfo::UsbBoardInfo()
{
    _loadJson();
}

void UsbBoardInfo::_loadJson()
{
    QFile file(":/USBBoardInfo.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("UsbBoardInfo: failed to open USBBoardInfo.json");
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    for (const QJsonValue& v : root["boardInfo"].toArray()) {
        const QJsonObject o = v.toObject();
        _boardInfo.append({
            static_cast<quint16>(o["vendorID"].toInt()),
            static_cast<quint16>(o["productID"].toInt()),
            o["boardClass"].toString(),
            o["name"].toString()
        });
    }

    for (const QJsonValue& v : root["boardDescriptionFallback"].toArray()) {
        const QJsonObject o = v.toObject();
        _descriptionFallback.append({ o["regExp"].toString(), o["boardClass"].toString() });
    }

    for (const QJsonValue& v : root["boardManufacturerFallback"].toArray()) {
        const QJsonObject o = v.toObject();
        _manufacturerFallback.append({ o["regExp"].toString(), o["boardClass"].toString() });
    }
}

bool UsbBoardInfo::isMavlinkBoard(const QSerialPortInfo& port) const
{
    // 1단계: VID/PID 매칭
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        for (const BoardEntry& entry : _boardInfo) {
            if (entry.vendorID == port.vendorIdentifier() &&
                (entry.productID == 0 || entry.productID == port.productIdentifier())) {
                return true;
            }
        }
    }

    // 2단계: 포트 설명 정규식 매칭
    for (const FallbackEntry& entry : _descriptionFallback) {
        if (QRegularExpression(entry.regExp).match(port.description()).hasMatch()) {
            return true;
        }
    }

    // 3단계: 제조사 정규식 매칭
    for (const FallbackEntry& entry : _manufacturerFallback) {
        if (QRegularExpression(entry.regExp).match(port.manufacturer()).hasMatch()) {
            return true;
        }
    }

    return false;
}

QString UsbBoardInfo::boardName(const QSerialPortInfo& port) const
{
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        for (const BoardEntry& entry : _boardInfo) {
            if (entry.vendorID == port.vendorIdentifier() &&
                (entry.productID == 0 || entry.productID == port.productIdentifier())) {
                return entry.name;
            }
        }
    }
    return port.description();
}
