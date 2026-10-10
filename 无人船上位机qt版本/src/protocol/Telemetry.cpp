#include "protocol/Telemetry.h"

#include <cmath>

namespace {
double num(const QJsonObject &obj, const char *key, double def = 0.0)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    if (v.isUndefined() || v.isNull()) {
        return def;
    }
    if (v.isDouble()) {
        return v.toDouble();
    }
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok ? d : def;
    }
    return v.toDouble(def);
}

int inum(const QJsonObject &obj, const char *key, int def = 0)
{
    return static_cast<int>(std::lround(num(obj, key, def)));
}

bool hasKey(const QJsonObject &obj, const char *key)
{
    return obj.contains(QLatin1String(key));
}
}

Telemetry Telemetry::fromJson(const QJsonObject &obj)
{
    Telemetry t;
    t.dataValid = inum(obj, "data_valid", 0);
    t.navReached = inum(obj, "nav_reached", 0);
    t.outputMoveSpeed = num(obj, "output_move_speed", t.outputMoveSpeed);
    t.outputSteerSpeed = num(obj, "output_steer_speed", t.outputSteerSpeed);
    t.heading = num(obj, "heading", 0.0);
    t.batteryLevel = num(obj, "battery_level", 0.0);

    if (hasKey(obj, "raw_lat") || hasKey(obj, "raw_lon")) {
        t.rawLat = num(obj, "raw_lat", 0.0);
        t.rawLon = num(obj, "raw_lon", 0.0);
    } else {
        /* 兼容旧协议字段名 */
        t.rawLat = num(obj, "dev_lat", 0.0);
        t.rawLon = num(obj, "dev_lon", 0.0);
    }

    if (hasKey(obj, "speed") && std::isfinite(num(obj, "speed"))) {
        t.speedMs = num(obj, "speed", 0.0);
    }

    if (hasKey(obj, "gnss_yaw") && std::isfinite(num(obj, "gnss_yaw"))) {
        t.gnssYaw = num(obj, "gnss_yaw");
    }
    if (hasKey(obj, "gz") && std::isfinite(num(obj, "gz"))) {
        t.gyroZ = num(obj, "gz");
    }

    if (hasKey(obj, "target_lat")) {
        t.targetLat = num(obj, "target_lat");
    }
    if (hasKey(obj, "target_lon")) {
        t.targetLon = num(obj, "target_lon");
    }
    if (hasKey(obj, "dist_error")) {
        t.distError = num(obj, "dist_error");
    }
    if (hasKey(obj, "heading_error")) {
        t.headingError = num(obj, "heading_error");
    }
    return t;
}
