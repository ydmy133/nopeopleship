#pragma once

#include <QJsonObject>
#include <QString>
#include <limits>

struct Telemetry {
    int dataValid = 0;
    int navReached = 0;
    double outputMoveSpeed = std::numeric_limits<double>::quiet_NaN();
    double outputSteerSpeed = std::numeric_limits<double>::quiet_NaN();
    double heading = 0.0;
    double batteryLevel = 0.0;
    double rawLat = 0.0;
    double rawLon = 0.0;
    double speedMs = 0.0;
    double gnssYaw = std::numeric_limits<double>::quiet_NaN();
    double gyroZ = std::numeric_limits<double>::quiet_NaN(); /**< 协议字段 gz，°/s */
    double targetLat = std::numeric_limits<double>::quiet_NaN();
    double targetLon = std::numeric_limits<double>::quiet_NaN();
    double distError = std::numeric_limits<double>::quiet_NaN();
    double headingError = std::numeric_limits<double>::quiet_NaN();

    bool gpsValid() const { return dataValid != 0; }

    static Telemetry fromJson(const QJsonObject &obj);
};
