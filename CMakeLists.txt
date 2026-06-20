CMAKE_MINIMUM_REQUIRED(VERSION 3.15)

PROJECT(HolyUUV_GCS VERSION 1.1.1)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_BUILD_TYPE Debug)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

STRING(TIMESTAMP BUILD_DATE "%Y%m%d")
ADD_COMPILE_DEFINITIONS(HOLYUUV_GCS_BUILD_DATE="${BUILD_DATE}")

# ─────────────────────────────────────────────
# 버전: 단일 소스 = 위 PROJECT(... VERSION x.y.z).
# 이 버전으로 version.h.in 템플릿에서 version.h 를 생성해 C++ 코드가 불러온다.
# 버전을 바꿀 땐 PROJECT() 한 줄만 (또는 ./scripts/set_version.sh) 수정하면 된다.
# ─────────────────────────────────────────────
CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info/version.h.in
    ${CMAKE_BINARY_DIR}/generated/info/version.h
    @ONLY
)
MESSAGE(STATUS "Version: ${PROJECT_VERSION}")

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Project: ${PROJECT_NAME}")

# ─────────────────────────────────────────────
# Qt5
# ─────────────────────────────────────────────
FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Core Widgets Network Location Quick QuickWidgets Positioning Sql
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
    src/util/log/crash_handler.cpp
    src/util/log/logger.cpp
    src/application/Application.cpp
    src/comm/ILink.cpp
    src/vehicle/VehicleState.cpp
    src/vehicle/VehicleManager.cpp
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
    ${CMAKE_BINARY_DIR}/generated   # CMake가 생성한 info/version.h 가 여기 있다
)

# 엄격한 경고로 빌드 (코드 품질 점검)
TARGET_COMPILE_OPTIONS(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)

# 크래시 핸들러 백트레이스에 함수명이 보이도록 동적 심볼 테이블 export
TARGET_LINK_OPTIONS(${PROJECT_NAME} PRIVATE -rdynamic)

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE
    ${MAVLINK_INCLUDE_DIR}
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
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
