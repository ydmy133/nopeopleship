#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

struct ControlCommand {
    QString controlMode = QStringLiteral("manual");
    int moveSpeed = 0;
    int steerSpeed = 0;
    double targetLat = 0.0;
    double targetLon = 0.0;
    double returnLat = 0.0;
    double returnLon = 0.0;
    int dcProt = 1;
    int lbProt = 1;

    void sanitize();
    QJsonObject toJson() const;
    QByteArray toUtf8Json() const;
    QString summaryLine() const;
    static ControlCommand fromJson(const QJsonObject &obj);
    static ControlCommand fromUtf8Json(const QByteArray &json);
    static QString summaryFromUtf8(const QByteArray &json);
    static bool isValidMode(const QString &mode);
};
