#pragma once

#include "protocol/Telemetry.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

struct TelemetryParseResult {
    bool ok = false;
    bool envelope = false;
    bool crcOk = true;
    Telemetry telemetry;
    QByteArray jsonBody;
    qint64 seq = -1;
    qint64 tsMs = -1;
    QString error;
};

namespace JsonCodec {
QByteArray sortedCompactJson(const QJsonObject &obj);
quint32 envelopeCrc32(int protoVer, qint64 seq, qint64 tsMs, const QJsonObject &payload);
TelemetryParseResult parseTelemetryMessage(const QByteArray &raw);
}
