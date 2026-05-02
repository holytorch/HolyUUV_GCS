#pragma once

#include <QObject>
#include "comm/LinkManager.h"
#include "mavlink/MavlinkManager.h"
#include "vehicle/VehicleState.h"
#include "ui/MainWindow.h"

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject* parent = nullptr);

private:
    LinkManager    _linkManager;
    MavlinkManager _mavlinkManager;
    VehicleState   _vehicleState;
    MainWindow     _mainWindow{&_vehicleState};
};
