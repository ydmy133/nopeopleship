cmake_minimum_required(VERSION 3.16)
project(usv_gcs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/utf-8 /W3)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
set(_qt_comps Core Gui Quick Qml Network Sql Svg)
if(QT_VERSION_MAJOR EQUAL 5)
    list(APPEND _qt_comps QuickControls2)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${_qt_comps})

set(USV_SOURCES
    src/main.cpp
    src/app/UsvBridge.cpp
    src/app/AppLog.cpp
    src/app/AppPaths.cpp
    src/protocol/ControlCommand.cpp
    src/protocol/Telemetry.cpp
    src/protocol/JsonCodec.cpp
    src/protocol/Crc32.cpp
    src/protocol/Geo.cpp
    src/transport/MqttClient.cpp
    src/transport/MqttLink.cpp
    src/transport/UdpLink.cpp
    src/transport/LinkManager.cpp
    src/control/ControlLoop.cpp
    src/telemetry/TelemetryStore.cpp
    src/preset/PresetStore.cpp
    src/preset/ReturnPointStore.cpp
    src/settings/AppSettings.cpp
    src/map/MapController.cpp
    src/map/TileFetcher.cpp
    src/map/TileImageProvider.cpp
)

set(USV_HEADERS
    src/app/UsvBridge.h
    src/app/AppLog.h
    src/app/AppPaths.h
    src/protocol/ControlCommand.h
    src/protocol/Telemetry.h
    src/protocol/JsonCodec.h
    src/protocol/Crc32.h
    src/protocol/Geo.h
    src/transport/ILink.h
    src/transport/MqttClient.h
    src/transport/MqttLink.h
    src/transport/UdpLink.h
    src/transport/LinkManager.h
    src/control/ControlLoop.h
    src/telemetry/TelemetryStore.h
    src/preset/PresetStore.h
    src/preset/ReturnPointStore.h
    src/settings/AppSettings.h
    src/map/MapController.h
    src/map/TileFetcher.h
    src/map/TileImageProvider.h
)

# Use add_executable on both Qt5/Qt6. qt_add_executable() runs qmlimportscanner,
# which breaks on Windows when the source path contains non-ASCII characters.
add_executable(usv_gcs WIN32 ${USV_SOURCES} ${USV_HEADERS} resources.qrc)

target_include_directories(usv_gcs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(usv_gcs PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
)

if(QT_VERSION_MAJOR EQUAL 5)
    target_link_libraries(usv_gcs PRIVATE Qt5::QuickControls2)
endif()

set_target_properties(usv_gcs PROPERTIES
    OUTPUT_NAME "usv_gcs"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
