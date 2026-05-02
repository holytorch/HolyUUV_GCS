/****************************************************************************
** Meta object code from reading C++ file 'MavlinkManager.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/mavlink/MavlinkManager.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MavlinkManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MavlinkManager_t {
    QByteArrayData data[25];
    char stringdata0[318];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MavlinkManager_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MavlinkManager_t qt_meta_stringdata_MavlinkManager = {
    {
QT_MOC_LITERAL(0, 0, 14), // "MavlinkManager"
QT_MOC_LITERAL(1, 15, 17), // "heartbeatReceived"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 16), // "MavlinkHeartbeat"
QT_MOC_LITERAL(4, 51, 2), // "hb"
QT_MOC_LITERAL(5, 54, 16), // "attitudeReceived"
QT_MOC_LITERAL(6, 71, 15), // "MavlinkAttitude"
QT_MOC_LITERAL(7, 87, 3), // "att"
QT_MOC_LITERAL(8, 91, 17), // "sysStatusReceived"
QT_MOC_LITERAL(9, 109, 16), // "MavlinkSysStatus"
QT_MOC_LITERAL(10, 126, 6), // "status"
QT_MOC_LITERAL(11, 133, 22), // "scaledPressureReceived"
QT_MOC_LITERAL(12, 156, 21), // "MavlinkScaledPressure"
QT_MOC_LITERAL(13, 178, 8), // "pressure"
QT_MOC_LITERAL(14, 187, 14), // "vfrHudReceived"
QT_MOC_LITERAL(15, 202, 13), // "MavlinkVfrHud"
QT_MOC_LITERAL(16, 216, 3), // "hud"
QT_MOC_LITERAL(17, 220, 22), // "globalPositionReceived"
QT_MOC_LITERAL(18, 243, 21), // "MavlinkGlobalPosition"
QT_MOC_LITERAL(19, 265, 3), // "pos"
QT_MOC_LITERAL(20, 269, 14), // "gpsRawReceived"
QT_MOC_LITERAL(21, 284, 13), // "MavlinkGpsRaw"
QT_MOC_LITERAL(22, 298, 3), // "gps"
QT_MOC_LITERAL(23, 302, 10), // "parseBytes"
QT_MOC_LITERAL(24, 313, 4) // "data"

    },
    "MavlinkManager\0heartbeatReceived\0\0"
    "MavlinkHeartbeat\0hb\0attitudeReceived\0"
    "MavlinkAttitude\0att\0sysStatusReceived\0"
    "MavlinkSysStatus\0status\0scaledPressureReceived\0"
    "MavlinkScaledPressure\0pressure\0"
    "vfrHudReceived\0MavlinkVfrHud\0hud\0"
    "globalPositionReceived\0MavlinkGlobalPosition\0"
    "pos\0gpsRawReceived\0MavlinkGpsRaw\0gps\0"
    "parseBytes\0data"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MavlinkManager[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   54,    2, 0x06 /* Public */,
       5,    1,   57,    2, 0x06 /* Public */,
       8,    1,   60,    2, 0x06 /* Public */,
      11,    1,   63,    2, 0x06 /* Public */,
      14,    1,   66,    2, 0x06 /* Public */,
      17,    1,   69,    2, 0x06 /* Public */,
      20,    1,   72,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      23,    1,   75,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, 0x80000000 | 18,   19,
    QMetaType::Void, 0x80000000 | 21,   22,

 // slots: parameters
    QMetaType::Void, QMetaType::QByteArray,   24,

       0        // eod
};

void MavlinkManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MavlinkManager *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->heartbeatReceived((*reinterpret_cast< const MavlinkHeartbeat(*)>(_a[1]))); break;
        case 1: _t->attitudeReceived((*reinterpret_cast< const MavlinkAttitude(*)>(_a[1]))); break;
        case 2: _t->sysStatusReceived((*reinterpret_cast< const MavlinkSysStatus(*)>(_a[1]))); break;
        case 3: _t->scaledPressureReceived((*reinterpret_cast< const MavlinkScaledPressure(*)>(_a[1]))); break;
        case 4: _t->vfrHudReceived((*reinterpret_cast< const MavlinkVfrHud(*)>(_a[1]))); break;
        case 5: _t->globalPositionReceived((*reinterpret_cast< const MavlinkGlobalPosition(*)>(_a[1]))); break;
        case 6: _t->gpsRawReceived((*reinterpret_cast< const MavlinkGpsRaw(*)>(_a[1]))); break;
        case 7: _t->parseBytes((*reinterpret_cast< const QByteArray(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MavlinkManager::*)(const MavlinkHeartbeat & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::heartbeatReceived)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkAttitude & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::attitudeReceived)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkSysStatus & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::sysStatusReceived)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkScaledPressure & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::scaledPressureReceived)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkVfrHud & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::vfrHudReceived)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkGlobalPosition & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::globalPositionReceived)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (MavlinkManager::*)(const MavlinkGpsRaw & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MavlinkManager::gpsRawReceived)) {
                *result = 6;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MavlinkManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_MavlinkManager.data,
    qt_meta_data_MavlinkManager,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MavlinkManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MavlinkManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MavlinkManager.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MavlinkManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void MavlinkManager::heartbeatReceived(const MavlinkHeartbeat & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void MavlinkManager::attitudeReceived(const MavlinkAttitude & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void MavlinkManager::sysStatusReceived(const MavlinkSysStatus & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void MavlinkManager::scaledPressureReceived(const MavlinkScaledPressure & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void MavlinkManager::vfrHudReceived(const MavlinkVfrHud & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void MavlinkManager::globalPositionReceived(const MavlinkGlobalPosition & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void MavlinkManager::gpsRawReceived(const MavlinkGpsRaw & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
