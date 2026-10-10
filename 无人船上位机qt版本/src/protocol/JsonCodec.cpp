#include "protocol/JsonCodec.h"

#include "protocol/Crc32.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>

namespace {

QJsonValue sortedValue(const QJsonValue &v);

QJsonObject sortObject(const QJsonObject &obj)
{
    QMap<QString, QJsonValue> ordered;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        ordered.insert(it.key(), sortedValue(it.value()));
    }
    QJsonObject out;
    for (auto it = ordered.begin(); it != ordered.end(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

QJsonValue sortedValue(const QJsonValue &v)
{
    if (v.isObject()) {
        return sortObject(v.toObject());
    }
    if (v.isArray()) {
        QJsonArray arr;
        const auto in = v.toArray();
        for (const auto &item : in) {
            arr.append(sortedValue(item));
        }
        return arr;
    }
    return v;
}

}

namespace JsonCodec {

QByteArray sortedCompactJson(const QJsonObject &obj)
{
    return QJsonDocument(sortObject(obj)).toJson(QJsonDocument::Compact);
}

quint32 envelopeCrc32(int protoVer, qint64 seq, qint64 tsMs, const QJsonObject &payload)
{
    const QByteArray payloadJson = sortedCompactJson(payload);
    const QByteArray base = QByteArray::number(protoVer) + '|'
        + QByteArray::number(seq) + '|'
        + QByteArray::number(tsMs) + '|'
        + payloadJson;
    return Crc32::ieee(base);
}

TelemetryParseResult parseTelemetryMessage(const QByteArray &raw)
{
    TelemetryParseResult result;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = err.errorString().isEmpty()
            ? QStringLiteral("invalid json")
            : err.errorString();
        return result;
    }

    const QJsonObject root = doc.object();
    const bool looksEnvelope = root.contains(QLatin1String("payload"))
        && root.contains(QLatin1String("proto_ver"))
        && root.contains(QLatin1String("seq"))
        && root.contains(QLatin1String("ts_ms"))
        && root.contains(QLatin1String("crc32"))
        && root.value(QLatin1String("payload")).isObject();

    if (looksEnvelope) {
        result.envelope = true;
        const int protoVer = root.value(QLatin1String("proto_ver")).toInt();
        const qint64 seq = static_cast<qint64>(root.value(QLatin1String("seq")).toDouble());
        const qint64 tsMs = static_cast<qint64>(root.value(QLatin1String("ts_ms")).toDouble());
        const quint32 actualCrc = static_cast<quint32>(
            static_cast<qint64>(root.value(QLatin1String("crc32")).toDouble()));
        const QJsonObject payload = root.value(QLatin1String("payload")).toObject();
        const quint32 expected = envelopeCrc32(protoVer, seq, tsMs, payload);
        result.seq = seq;
        result.tsMs = tsMs;
        if (expected != actualCrc) {
            result.crcOk = false;
            result.error = QStringLiteral("crc32 mismatch");
            return result;
        }
        result.jsonBody = QJsonDocument(payload).toJson(QJsonDocument::Compact).trimmed();
        result.telemetry = Telemetry::fromJson(payload);
        result.ok = true;
        return result;
    }

    result.jsonBody = QJsonDocument(root).toJson(QJsonDocument::Compact).trimmed();
    result.telemetry = Telemetry::fromJson(root);
    result.ok = true;
    return result;
}

} // namespace JsonCodec
