#include "UsbBoardInfo.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
// instance()
// 프로세스 전체에서 하나의 UsbBoardInfo 인스턴스를 반환한다 (Meyer의 싱글턴).
// ─────────────────────────────────────────────────────────────────────────────
UsbBoardInfo& UsbBoardInfo::instance()
{
    static UsbBoardInfo inst;
    return inst;
}


// ─────────────────────────────────────────────────────────────────────────────
// UsbBoardInfo()
// 생성 시 JSON을 로드한다.
// ─────────────────────────────────────────────────────────────────────────────
UsbBoardInfo::UsbBoardInfo()
{
    _loadJson();
}


// ─────────────────────────────────────────────────────────────────────────────
// _loadJson()
// :/USBBoardInfo.json(Qt 리소스)을 파싱해 boardInfo / 두 fallback 목록을 채운다.
// ─────────────────────────────────────────────────────────────────────────────
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


// ─────────────────────────────────────────────────────────────────────────────
// isMavlinkBoard()
// VID/PID → 포트 설명 정규식 → 제조사 정규식 순서로 MAVLink 보드 여부를 판단한다.
// productID가 0인 항목은 해당 vendorID의 모든 제품에 매칭된다.
// ─────────────────────────────────────────────────────────────────────────────
bool UsbBoardInfo::isMavlinkBoard(const QSerialPortInfo& port) const
{
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        for (const BoardEntry& entry : _boardInfo) {
            if (entry.vendorID == port.vendorIdentifier() &&
                (entry.productID == 0 || entry.productID == port.productIdentifier()))
                return true;
        }
    }

    for (const FallbackEntry& entry : _descriptionFallback) {
        if (QRegularExpression(entry.regExp).match(port.description()).hasMatch())
            return true;
    }

    for (const FallbackEntry& entry : _manufacturerFallback) {
        if (QRegularExpression(entry.regExp).match(port.manufacturer()).hasMatch())
            return true;
    }

    return false;
}


// ─────────────────────────────────────────────────────────────────────────────
// boardName()
// VID/PID로 보드 이름을 반환한다. 매칭 실패 시 포트 설명 문자열을 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
QString UsbBoardInfo::boardName(const QSerialPortInfo& port) const
{
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        for (const BoardEntry& entry : _boardInfo) {
            if (entry.vendorID == port.vendorIdentifier() &&
                (entry.productID == 0 || entry.productID == port.productIdentifier()))
                return entry.name;
        }
    }
    return port.description();
}
