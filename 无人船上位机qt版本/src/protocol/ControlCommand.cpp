#include "protocol/ControlCommand.h"

#include "protocol/Geo.h"

#include <QJsonDocument>
#include <QSet>

namespace {
const QSet<QString> kModes = {
    QStringLiteral("manual"),
    QStringLiteral("navigate"),
    QStringLiteral("stable_anchor"),
    QStringLiteral("cruise_speed"),
    QStringLiteral("cruise_dir"),
    QStringLiteral("track_hold"),
};
}

bool ControlCommand::isValidMode(const QString &mode)
{
    return kModes.contains(mode);
}

void ControlCommand::sanitize()
{
    if (!isValidMode(controlMode)) {
        controlMode = QStringLiteral("manual");
    }
    moveSpeed = Geo::clampInt(moveSpeed, -100, 100);
    steerSpeed = Geo::clampInt(steerSpeed, -100, 100);
    targetLat = Geo::clampDouble(targetLat, -90.0, 90.0);
    targetLon = Geo::clampDouble(targetLon, -180.0, 180.0);
    returnLat = Geo::clampDouble(returnLat, -90.0, 90.0);
    returnLon = Geo::clampDouble(returnLon, -180.0, 180.0);
    dcProt = Geo::clampInt(dcProt, 0, 1);
    lbProt = Geo::clampInt(lbProt, 0, 1);
}

QJsonObject ControlCommand::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("control_mode"), controlMode);
    o.insert(QStringLiteral("move_speed"), moveSpeed);
    o.insert(QStringLiteral("steer_speed"), steerSpeed);
    o.insert(QStringLiteral("target_lat"), targetLat);
    o.insert(QStringLiteral("target_lon"), targetLon);
    o.insert(QStringLiteral("return_lat"), returnLat);
    o.insert(QStringLiteral("return_lon"), returnLon);
    o.insert(QStringLiteral("dc_prot"), dcProt);
    o.insert(QStringLiteral("lb_prot"), lbProt);
    return o;
}

QByteArray ControlCommand::toUtf8Json() const
{
    ControlCommand copy = *this;
    copy.sanitize();
    return QJsonDocument(copy.toJson()).toJson(QJsonDocument::Compact);
}

QString ControlCommand::summaryLine() const
{
    return QStringLiteral("control_mode=%1, move_speed=%2, steer_speed=%3, target=(%4,%5), return=(%6,%7), dc_prot=%8, lb_prot=%9")
        .arg(controlMode)
        .arg(moveSpeed)
        .arg(steerSpeed)
        .arg(targetLat, 0, 'f', 6)
        .arg(targetLon, 0, 'f', 6)
        .arg(returnLat, 0, 'f', 6)
        .arg(returnLon, 0, 'f', 6)
        .arg(dcProt)
        .arg(lbProt);
}

ControlCommand ControlCommand::fromUtf8Json(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return {};
    }
    return fromJson(doc.object());
}

QString ControlCommand::summaryFromUtf8(const QByteArray &json)
{
    return fromUtf8Json(json).summaryLine();
}

ControlCommand ControlCommand::fromJson(const QJsonObject &obj)
{
    ControlCommand c;
    c.controlMode = obj.value(QStringLiteral("control_mode")).toString(QStringLiteral("manual"));
    c.moveSpeed = obj.value(QStringLiteral("move_speed")).toInt(0);
    c.steerSpeed = obj.value(QStringLiteral("steer_speed")).toInt(0);
    c.targetLat = obj.value(QStringLiteral("target_lat")).toDouble(0.0);
    c.targetLon = obj.value(QStringLiteral("target_lon")).toDouble(0.0);
    c.returnLat = obj.value(QStringLiteral("return_lat")).toDouble(0.0);
    c.returnLon = obj.value(QStringLiteral("return_lon")).toDouble(0.0);
    c.dcProt = obj.contains(QStringLiteral("dc_prot")) ? obj.value(QStringLiteral("dc_prot")).toInt(1) : 1;
    c.lbProt = obj.contains(QStringLiteral("lb_prot")) ? obj.value(QStringLiteral("lb_prot")).toInt(1) : 1;
    c.sanitize();
    return c;
}
