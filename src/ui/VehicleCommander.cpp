#include "VehicleCommander.h"

#include <QMap>
#include <QDebug>


VehicleCommander::VehicleCommander(QObject* parent) : QObject(parent)
{
    qInfo("[init] VehicleCommander");
}


VehicleCommander::~VehicleCommander()
{
    qInfo("[exit] VehicleCommander");
}


void VehicleCommander::setArm(bool arm)
{
    qInfo("Commander: %s requested", arm ? "ARM" : "DISARM");
    emit armRequested(arm);
}


void VehicleCommander::setMode(const QString& modeName)
{
    // ArduSub custom_mode 매핑. ArduPilot 펌웨어 정의 그대로.
    static const QMap<QString, uint32_t> kModeMap = {
        {"STABILIZE",     0},
        {"ACRO",          1},
        {"ALT_HOLD",      2},
        {"AUTO",          3},
        {"GUIDED",        4},
        {"CIRCLE",        7},
        {"SURFACE",       9},
        {"POSHOLD",      16},
        {"MANUAL",       19},
        {"MOTOR_DETECT", 20},
    };

    const QString key = modeName.toUpper();
    auto it = kModeMap.find(key);
    if (it == kModeMap.end()) {
        qWarning("Commander: unknown mode '%s'", qPrintable(modeName));
        return;
    }

    qInfo("Commander: setMode %s (%u)", qPrintable(key), it.value());
    emit setModeRequested(it.value());
}


void VehicleCommander::sendManualControl(int x, int y, int z, int r, int buttons)
{
    // int → int16_t/uint16_t 안전 캐스트. QML은 int만 전달하므로 여기서 clamp.
    auto clamp16 = [](int v) {
        if (v < -32768) v = -32768;
        if (v > 32767)  v = 32767;
        return static_cast<int16_t>(v);
    };
    emit manualControlRequested(
        clamp16(x), clamp16(y), clamp16(z), clamp16(r),
        static_cast<uint16_t>(buttons & 0xFFFF));
}
