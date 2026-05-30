CMAKE_MINIMUM_REQUIRED(VERSION 3.15)

PROJECT(HolyUUV_GCS)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_BUILD_TYPE Debug)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

STRING(TIMESTAMP BUILD_DATE "%Y%m%d")
ADD_COMPILE_DEFINITIONS(HOLYUUV_GCS_BUILD_DATE="${BUILD_DATE}")

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Project: ${PROJECT_NAME}")

# ─────────────────────────────────────────────
# Qt5
# ─────────────────────────────────────────────
FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Core Widgets Network SerialPort Location Quick QuickWidgets Positioning Sql
             3DCore 3DRender 3DExtras 3DInput)

# ─────────────────────────────────────────────
# MAVLink (header-only, c_library_v2)
# Run ./setup_mavlink.sh to clone headers
# ─────────────────────────────────────────────
SET(MAVLINK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mavlink/include)
IF(EXISTS "${MAVLINK_INCLUDE_DIR}/mavlink/v2.0/ardupilotmega/mavlink.h")
    MESSAGE(STATUS "MAVLink headers found")
    ADD_COMPILE_DEFINITIONS(MAVLINK_AVAILABLE)
ELSE()
    MESSAGE(WARNING "MAVLink headers NOT found at ${MAVLINK_INCLUDE_DIR}/mavlink/v2.0/")
    MESSAGE(WARNING "Run ./setup_mavlink.sh to download them")
ENDIF()

# ─────────────────────────────────────────────
# Sources
# ─────────────────────────────────────────────
SET(SOURCES
    src/main.cpp
    src/application/Application.cpp
    src/comm/ILink.cpp
    src/vehicle/VehicleState.cpp
    src/ui/MainWindow.cpp
    src/comm/LinkManager.cpp
    src/comm/UdpLink.cpp
    src/mavlink/MavlinkManager.cpp
    src/ui/MapBridge.cpp
    src/ui/LogFeed.cpp
    src/ui/VehicleCommander.cpp
    src/ui/ConnectionBridge.cpp
    src/map/TileCache.cpp
    src/map/TileCacheWorker.cpp
    src/map/TileServer.cpp
    src/terrain/TerrainTile.cpp
    src/terrain/TerrainScene.cpp
)

SET(RESOURCES
    resources/resources.qrc
)

ADD_EXECUTABLE(${PROJECT_NAME} ${SOURCES} ${RESOURCES})

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE
    src
    src/terrain
)

# MAVLink은 서드파티 헤더라 경고 억제
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PRIVATE
    ${MAVLINK_INCLUDE_DIR}
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::SerialPort
    Qt5::Location
    Qt5::Quick
    Qt5::QuickWidgets
    Qt5::Positioning
    Qt5::Sql
    Qt5::3DCore
    Qt5::3DRender
    Qt5::3DExtras
    Qt5::3DInput
)
