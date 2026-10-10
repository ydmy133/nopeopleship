#include "settings/AppSettings.h"

#include "app/AppPaths.h"
#include "protocol/Geo.h"

#include <QSettings>
#include <QTimer>
#include <QUuid>

namespace {

QString normalizeMqttScheme(QString raw)
{
    raw = raw.trimmed().toLower();
    if (raw.endsWith(QLatin1String("://"))) {
        raw.chop(3);
    }
    if (raw == QLatin1String("ssl") || raw == QLatin1String("tls") || raw == QLatin1String("mqtts")) {
        return QStringLiteral("mqtts");
    }
    if (raw == QLatin1String("ws")) {
        return QStringLiteral("ws");
    }
    if (raw == QLatin1String("wss")) {
        return QStringLiteral("wss");
    }
    return QStringLiteral("mqtt");
}

int defaultPortForScheme(const QString &scheme)
{
    if (scheme == QLatin1String("mqtts")) {
        return 8883;
    }
    if (scheme == QLatin1String("ws")) {
        return 8083;
    }
    if (scheme == QLatin1String("wss")) {
        return 8084;
    }
    return 1883;
}

bool isDefaultPortForScheme(const QString &scheme, int port)
{
    return port == defaultPortForScheme(scheme);
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(250);
    connect(m_saveTimer, &QTimer::timeout, this, &AppSettings::saveNow);
    load();
}

AppSettings::~AppSettings()
{
    if (m_saveTimer && m_saveTimer->isActive()) {
        saveNow();
    }
}

void AppSettings::load()
{
    QSettings s(AppPaths::settingsFile(), QSettings::IniFormat);
    m_commMode = s.value(QStringLiteral("comm/mode"), m_commMode).toString();
    if (m_commMode != QLatin1String("udp")) {
        m_commMode = QStringLiteral("mqtt");
    }
    m_mqttHost = s.value(QStringLiteral("mqtt/host"), m_mqttHost).toString();
    m_mqttPort = s.value(QStringLiteral("mqtt/port"), m_mqttPort).toInt();
    m_mqttSsl = s.value(QStringLiteral("mqtt/ssl"), m_mqttSsl).toBool();
    m_mqttSslInsecure = s.value(QStringLiteral("mqtt/sslInsecure"), m_mqttSslInsecure).toBool();
    m_mqttScheme = normalizeMqttScheme(s.value(QStringLiteral("mqtt/scheme")).toString());
    if (!s.contains(QStringLiteral("mqtt/scheme"))) {
        m_mqttScheme = m_mqttSsl ? QStringLiteral("mqtts") : QStringLiteral("mqtt");
    }
    m_mqttSsl = (m_mqttScheme == QLatin1String("mqtts") || m_mqttScheme == QLatin1String("wss"));
    m_mqttUsername = s.value(QStringLiteral("mqtt/username"), m_mqttUsername).toString();
    m_mqttPassword = s.value(QStringLiteral("mqtt/password"), m_mqttPassword).toString();
    m_mqttClientId = s.value(QStringLiteral("mqtt/clientId")).toString();
    if (m_mqttClientId.isEmpty()) {
        m_mqttClientId = QStringLiteral("usv-qt-gcs-%1")
                             .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
        s.setValue(QStringLiteral("mqtt/clientId"), m_mqttClientId);
    }
    m_mqttBoatId = s.value(QStringLiteral("mqtt/boatId"), m_mqttBoatId).toString();
    m_mqttPublishTopic = s.value(QStringLiteral("mqtt/publishTopic")).toString().trimmed();
    if (m_mqttPublishTopic.isEmpty()) {
        m_mqttPublishTopic = QStringLiteral("usv/%1/cmd/control").arg(m_mqttBoatId);
    }
    m_mqttTelemetryTopic = s.value(QStringLiteral("mqtt/telemetryTopic"), m_mqttTelemetryTopic).toString();

    m_udpLocalHost = s.value(QStringLiteral("udp/localHost"), m_udpLocalHost).toString();
    m_udpLocalPort = s.value(QStringLiteral("udp/localPort"), m_udpLocalPort).toInt();
    m_udpRemoteHost = s.value(QStringLiteral("udp/remoteHost"), m_udpRemoteHost).toString();
    m_udpRemotePort = s.value(QStringLiteral("udp/remotePort"), m_udpRemotePort).toInt();

    m_dcProtOn = s.value(QStringLiteral("prot/dc"), true).toBool();
    m_lbProtOn = s.value(QStringLiteral("prot/lb"), true).toBool();

    m_hasReturn = s.value(QStringLiteral("return/has"), false).toBool();
    m_returnLat = s.value(QStringLiteral("return/lat"), 0.0).toDouble();
    m_returnLon = s.value(QStringLiteral("return/lon"), 0.0).toDouble();
    m_hasOneKey = s.value(QStringLiteral("oneKey/has"), false).toBool();
    m_oneKeyLat = s.value(QStringLiteral("oneKey/lat"), 0.0).toDouble();
    m_oneKeyLon = s.value(QStringLiteral("oneKey/lon"), 0.0).toDouble();
    m_activeReturnId = s.value(QStringLiteral("oneKey/id"), -1).toInt();
    if (!m_hasOneKey && m_hasReturn) {
        m_oneKeyLat = m_returnLat;
        m_oneKeyLon = m_returnLon;
        m_hasOneKey = true;
    }

    m_language = s.value(QStringLiteral("ui/language"), m_language).toString();
    m_theme = s.value(QStringLiteral("ui/theme"), m_theme).toString();
    m_displayUnit = s.value(QStringLiteral("ui/unit"), m_displayUnit).toString();
    m_dcpa = s.value(QStringLiteral("ui/dcpa"), m_dcpa).toString();
    m_tcpa = s.value(QStringLiteral("ui/tcpa"), m_tcpa).toString();
    m_alertVoice = s.value(QStringLiteral("ui/alertVoice"), true).toBool();
    m_flash = s.value(QStringLiteral("ui/flash"), true).toBool();
    m_aisLabel = s.value(QStringLiteral("ui/aisLabel"), true).toBool();
    m_moveSpeedPercent = normalizeSpeedPercent(
        s.value(QStringLiteral("ui/moveSpeedPercent"), kDefaultSpeedPercent).toInt());
    m_steerSpeedPercent = normalizeSpeedPercent(
        s.value(QStringLiteral("ui/steerSpeedPercent"), kDefaultSpeedPercent).toInt());
    if (m_moveSpeedPercent <= 0) {
        m_moveSpeedPercent = kDefaultSpeedPercent;
    }
    if (m_steerSpeedPercent <= 0) {
        m_steerSpeedPercent = kDefaultSpeedPercent;
    }

    m_mapType = s.value(QStringLiteral("map/type"), m_mapType).toString();
    if (m_mapType != QLatin1String("satellite")) {
        m_mapType = QStringLiteral("street");
    }
    m_mapBaseUrl = s.value(QStringLiteral("map/baseUrl"), m_mapBaseUrl).toString();
    if (m_mapBaseUrl.trimmed().isEmpty()) {
        m_mapBaseUrl = QStringLiteral("http://210.76.81.182");
    }
    m_hasMapLast = s.value(QStringLiteral("map/hasLast"), false).toBool();
    m_mapLastLat = s.value(QStringLiteral("map/lastLat"), m_mapLastLat).toDouble();
    m_mapLastLon = s.value(QStringLiteral("map/lastLon"), m_mapLastLon).toDouble();
    m_mapLastZoom = Geo::clampInt(s.value(QStringLiteral("map/lastZoom"), m_mapLastZoom).toInt(), 3, 18);
}

void AppSettings::save()
{
    if (m_saveTimer) {
        m_saveTimer->start();
        return;
    }
    saveNow();
}

void AppSettings::saveNow()
{
    if (m_saveTimer) {
        m_saveTimer->stop();
    }
    QSettings s(AppPaths::settingsFile(), QSettings::IniFormat);
    s.setValue(QStringLiteral("comm/mode"), m_commMode);
    s.setValue(QStringLiteral("mqtt/host"), m_mqttHost);
    s.setValue(QStringLiteral("mqtt/scheme"), m_mqttScheme);
    s.setValue(QStringLiteral("mqtt/port"), m_mqttPort);
    s.setValue(QStringLiteral("mqtt/ssl"), m_mqttSsl);
    s.setValue(QStringLiteral("mqtt/sslInsecure"), m_mqttSslInsecure);
    s.setValue(QStringLiteral("mqtt/username"), m_mqttUsername);
    s.setValue(QStringLiteral("mqtt/password"), m_mqttPassword);
    s.setValue(QStringLiteral("mqtt/clientId"), m_mqttClientId);
    s.setValue(QStringLiteral("mqtt/boatId"), m_mqttBoatId);
    s.setValue(QStringLiteral("mqtt/publishTopic"), m_mqttPublishTopic);
    s.setValue(QStringLiteral("mqtt/telemetryTopic"), m_mqttTelemetryTopic);
    s.setValue(QStringLiteral("udp/localHost"), m_udpLocalHost);
    s.setValue(QStringLiteral("udp/localPort"), m_udpLocalPort);
    s.setValue(QStringLiteral("udp/remoteHost"), m_udpRemoteHost);
    s.setValue(QStringLiteral("udp/remotePort"), m_udpRemotePort);
    s.setValue(QStringLiteral("prot/dc"), m_dcProtOn);
    s.setValue(QStringLiteral("prot/lb"), m_lbProtOn);
    s.setValue(QStringLiteral("return/has"), m_hasReturn);
    s.setValue(QStringLiteral("return/lat"), m_returnLat);
    s.setValue(QStringLiteral("return/lon"), m_returnLon);
    s.setValue(QStringLiteral("oneKey/has"), m_hasOneKey);
    s.setValue(QStringLiteral("oneKey/lat"), m_oneKeyLat);
    s.setValue(QStringLiteral("oneKey/lon"), m_oneKeyLon);
    s.setValue(QStringLiteral("oneKey/id"), m_activeReturnId);
    s.setValue(QStringLiteral("ui/language"), m_language);
    s.setValue(QStringLiteral("ui/theme"), m_theme);
    s.setValue(QStringLiteral("ui/unit"), m_displayUnit);
    s.setValue(QStringLiteral("ui/dcpa"), m_dcpa);
    s.setValue(QStringLiteral("ui/tcpa"), m_tcpa);
    s.setValue(QStringLiteral("ui/alertVoice"), m_alertVoice);
    s.setValue(QStringLiteral("ui/flash"), m_flash);
    s.setValue(QStringLiteral("ui/aisLabel"), m_aisLabel);
    s.setValue(QStringLiteral("ui/moveSpeedPercent"), m_moveSpeedPercent);
    s.setValue(QStringLiteral("ui/steerSpeedPercent"), m_steerSpeedPercent);
    s.setValue(QStringLiteral("map/type"), m_mapType);
    s.setValue(QStringLiteral("map/baseUrl"), m_mapBaseUrl);
    s.setValue(QStringLiteral("map/hasLast"), m_hasMapLast);
    s.setValue(QStringLiteral("map/lastLat"), m_mapLastLat);
    s.setValue(QStringLiteral("map/lastLon"), m_mapLastLon);
    s.setValue(QStringLiteral("map/lastZoom"), m_mapLastZoom);
}

void AppSettings::setCommMode(const QString &v)
{
    const QString mode = (v == QLatin1String("udp")) ? QStringLiteral("udp") : QStringLiteral("mqtt");
    if (m_commMode == mode) {
        return;
    }
    m_commMode = mode;
    save();
    emit commModeChanged();
}

void AppSettings::setMqttHost(const QString &v)
{
    QString host = v.trimmed();
    QString scheme = m_mqttScheme;
    const struct {
        const char *prefix;
        const char *name;
    } kPrefixes[] = {
        {"mqtts://", "mqtts"},
        {"mqtt://", "mqtt"},
        {"wss://", "wss"},
        {"ws://", "ws"},
    };
    for (const auto &p : kPrefixes) {
        const QString prefix = QString::fromLatin1(p.prefix);
        if (host.startsWith(prefix, Qt::CaseInsensitive)) {
            scheme = QString::fromLatin1(p.name);
            host = host.mid(prefix.size());
            break;
        }
    }
    while (host.endsWith(QLatin1Char('/'))) {
        host.chop(1);
    }
    if (host == m_mqttHost && scheme == m_mqttScheme) {
        return;
    }
    m_mqttHost = host;
    if (scheme != m_mqttScheme) {
        setMqttScheme(scheme);
        return;
    }
    save();
    emit mqttChanged();
}

void AppSettings::setMqttScheme(const QString &v)
{
    const QString scheme = normalizeMqttScheme(v);
    if (scheme == m_mqttScheme) {
        m_mqttSsl = (scheme == QLatin1String("mqtts") || scheme == QLatin1String("wss"));
        return;
    }
    const bool keepPort = !isDefaultPortForScheme(m_mqttScheme, m_mqttPort);
    m_mqttScheme = scheme;
    m_mqttSsl = (scheme == QLatin1String("mqtts") || scheme == QLatin1String("wss"));
    if (!keepPort) {
        m_mqttPort = defaultPortForScheme(scheme);
    }
    save();
    emit mqttChanged();
}

bool AppSettings::mqttUsesWebSocket() const
{
    return m_mqttScheme == QLatin1String("ws") || m_mqttScheme == QLatin1String("wss");
}

void AppSettings::setMqttPort(int v)
{
    v = Geo::clampInt(v, 1, 65535);
    if (m_mqttPort == v) return;
    m_mqttPort = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttSsl(bool v)
{
    const bool ws = mqttUsesWebSocket();
    setMqttScheme(ws ? (v ? QStringLiteral("wss") : QStringLiteral("ws"))
                     : (v ? QStringLiteral("mqtts") : QStringLiteral("mqtt")));
}
void AppSettings::setMqttSslInsecure(bool v)
{
    if (m_mqttSslInsecure == v) return;
    m_mqttSslInsecure = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttUsername(const QString &v)
{
    if (m_mqttUsername == v) return;
    m_mqttUsername = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttPassword(const QString &v)
{
    if (m_mqttPassword == v) return;
    m_mqttPassword = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttClientId(const QString &v)
{
    if (v.isEmpty() || m_mqttClientId == v) return;
    m_mqttClientId = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttBoatId(const QString &v)
{
    if (m_mqttBoatId == v) return;
    m_mqttBoatId = v;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttPublishTopic(const QString &v)
{
    const QString t = v.trimmed();
    if (t.isEmpty() || m_mqttPublishTopic == t) return;
    m_mqttPublishTopic = t;
    save();
    emit mqttChanged();
}
void AppSettings::setMqttTelemetryTopic(const QString &v)
{
    if (m_mqttTelemetryTopic == v) return;
    m_mqttTelemetryTopic = v;
    save();
    emit mqttChanged();
}

void AppSettings::setUdpLocalHost(const QString &v)
{
    if (m_udpLocalHost == v) return;
    m_udpLocalHost = v;
    save();
    emit udpChanged();
}
void AppSettings::setUdpLocalPort(int v)
{
    v = Geo::clampInt(v, 1, 65535);
    if (m_udpLocalPort == v) return;
    m_udpLocalPort = v;
    save();
    emit udpChanged();
}
void AppSettings::setUdpRemoteHost(const QString &v)
{
    if (m_udpRemoteHost == v) return;
    m_udpRemoteHost = v;
    save();
    emit udpChanged();
}
void AppSettings::setUdpRemotePort(int v)
{
    v = Geo::clampInt(v, 1, 65535);
    if (m_udpRemotePort == v) return;
    m_udpRemotePort = v;
    save();
    emit udpChanged();
}

void AppSettings::setDcProtOn(bool v)
{
    if (m_dcProtOn == v) return;
    m_dcProtOn = v;
    save();
    emit protectionChanged();
}
void AppSettings::setLbProtOn(bool v)
{
    if (m_lbProtOn == v) return;
    m_lbProtOn = v;
    save();
    emit protectionChanged();
}

void AppSettings::setReturnLat(double v)
{
    setReturnPoint(v, m_returnLon);
}
void AppSettings::setReturnLon(double v)
{
    setReturnPoint(m_returnLat, v);
}
void AppSettings::setReturnPoint(double lat, double lon)
{
    setOneKeyPoint(lat, lon);
}
void AppSettings::clearReturnPoint()
{
    m_hasReturn = false;
    m_returnLat = 0;
    m_returnLon = 0;
    m_hasOneKey = false;
    m_oneKeyLat = 0;
    m_oneKeyLon = 0;
    m_activeReturnId = -1;
    save();
    emit returnChanged();
    emit oneKeyChanged();
}

void AppSettings::setActiveReturnId(int id)
{
    if (m_activeReturnId == id) {
        return;
    }
    m_activeReturnId = id;
    save();
    emit oneKeyChanged();
}

void AppSettings::setOneKeyLat(double v)
{
    setOneKeyPoint(v, m_oneKeyLon, m_activeReturnId);
}
void AppSettings::setOneKeyLon(double v)
{
    setOneKeyPoint(m_oneKeyLat, v, m_activeReturnId);
}
void AppSettings::setOneKeyPoint(double lat, double lon, int returnId)
{
    m_oneKeyLat = Geo::clampDouble(lat, -90.0, 90.0);
    m_oneKeyLon = Geo::clampDouble(lon, -180.0, 180.0);
    m_hasOneKey = true;
    m_returnLat = m_oneKeyLat;
    m_returnLon = m_oneKeyLon;
    m_hasReturn = true;
    if (returnId >= 0) {
        m_activeReturnId = returnId;
    }
    save();
    emit oneKeyChanged();
    emit returnChanged();
}

void AppSettings::setLanguage(const QString &v)
{
    if (m_language == v) return;
    m_language = v;
    save();
    emit uiChanged();
}
void AppSettings::setTheme(const QString &v)
{
    if (m_theme == v) return;
    m_theme = v;
    save();
    emit uiChanged();
}
void AppSettings::setDisplayUnit(const QString &v)
{
    if (m_displayUnit == v) return;
    m_displayUnit = v;
    save();
    emit uiChanged();
}
void AppSettings::setDcpa(const QString &v)
{
    if (m_dcpa == v) return;
    m_dcpa = v;
    save();
    emit uiChanged();
}
void AppSettings::setTcpa(const QString &v)
{
    if (m_tcpa == v) return;
    m_tcpa = v;
    save();
    emit uiChanged();
}
void AppSettings::setAlertVoice(bool v)
{
    if (m_alertVoice == v) return;
    m_alertVoice = v;
    save();
    emit uiChanged();
}
void AppSettings::setFlash(bool v)
{
    if (m_flash == v) return;
    m_flash = v;
    save();
    emit uiChanged();
}
void AppSettings::setAisLabel(bool v)
{
    if (m_aisLabel == v) return;
    m_aisLabel = v;
    save();
    emit uiChanged();
}

int AppSettings::normalizeSpeedPercent(int v) const
{
    return Geo::clampInt(v, 0, 100);
}

int AppSettings::commandSpeedFromPercent(int pct) const
{
    /* 滑条 0–100 与协议 move_speed/steer_speed 一一对应 */
    return normalizeSpeedPercent(pct);
}

void AppSettings::setMoveSpeedPercent(int v)
{
    v = normalizeSpeedPercent(v);
    if (m_moveSpeedPercent == v) return;
    m_moveSpeedPercent = v;
    save();
    emit motorChanged();
}

void AppSettings::setSteerSpeedPercent(int v)
{
    v = normalizeSpeedPercent(v);
    if (m_steerSpeedPercent == v) return;
    m_steerSpeedPercent = v;
    save();
    emit motorChanged();
}

int AppSettings::speedPercent() const
{
    return m_moveSpeedPercent;
}

int AppSettings::moveCommandSpeed() const
{
    return commandSpeedFromPercent(m_moveSpeedPercent);
}

int AppSettings::steerCommandSpeed() const
{
    return commandSpeedFromPercent(m_steerSpeedPercent);
}

void AppSettings::cycleLanguage()
{
    setLanguage(m_language == QLatin1String("zh") ? QStringLiteral("en") : QStringLiteral("zh"));
}

void AppSettings::cycleTheme()
{
    setTheme(m_theme == QLatin1String("day") ? QStringLiteral("night") : QStringLiteral("day"));
}

void AppSettings::cycleDisplayUnit()
{
    if (m_displayUnit == QLatin1String("nm")) {
        setDisplayUnit(QStringLiteral("km"));
    } else if (m_displayUnit == QLatin1String("km")) {
        setDisplayUnit(QStringLiteral("mi"));
    } else {
        setDisplayUnit(QStringLiteral("nm"));
    }
}

QString AppSettings::displayUnitName() const
{
    if (m_displayUnit == QLatin1String("km")) {
        return m_language == QLatin1String("en") ? QStringLiteral("Kilometer") : QStringLiteral("千米");
    }
    if (m_displayUnit == QLatin1String("mi")) {
        return m_language == QLatin1String("en") ? QStringLiteral("Mile") : QStringLiteral("英里");
    }
    return m_language == QLatin1String("en") ? QStringLiteral("Nautical mile") : QStringLiteral("海里");
}

QString AppSettings::languageName() const
{
    return m_language == QLatin1String("en") ? QStringLiteral("English") : QStringLiteral("中文");
}

QString AppSettings::themeName() const
{
    if (m_language == QLatin1String("en")) {
        return m_theme == QLatin1String("night") ? QStringLiteral("Night") : QStringLiteral("Day");
    }
    return m_theme == QLatin1String("night") ? QStringLiteral("夜间模式") : QStringLiteral("日间模式");
}

void AppSettings::setMapType(const QString &v)
{
    const QString type = (v == QLatin1String("satellite")) ? QStringLiteral("satellite")
                                                          : QStringLiteral("street");
    if (m_mapType == type) {
        return;
    }
    m_mapType = type;
    save();
    emit mapChanged();
}

void AppSettings::setMapBaseUrl(const QString &v)
{
    QString url = v.trimmed();
    if (url.isEmpty()) {
        url = QStringLiteral("http://210.76.81.182");
    }
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    if (m_mapBaseUrl == url) {
        return;
    }
    m_mapBaseUrl = url;
    save();
    emit mapChanged();
}

void AppSettings::setMapLastView(double lat, double lon, int zoom)
{
    lat = Geo::clampDouble(lat, -90.0, 90.0);
    lon = Geo::clampDouble(lon, -180.0, 180.0);
    zoom = Geo::clampInt(zoom, 3, 18);
    if (m_hasMapLast && qFuzzyCompare(m_mapLastLat + 1.0, lat + 1.0)
        && qFuzzyCompare(m_mapLastLon + 1.0, lon + 1.0) && m_mapLastZoom == zoom) {
        return;
    }
    m_mapLastLat = lat;
    m_mapLastLon = lon;
    m_mapLastZoom = zoom;
    m_hasMapLast = true;
    save();
}

void AppSettings::cycleMapType()
{
    setMapType(m_mapType == QLatin1String("satellite") ? QStringLiteral("street")
                                                      : QStringLiteral("satellite"));
}

QString AppSettings::mapTypeName() const
{
    const bool sat = m_mapType == QLatin1String("satellite");
    if (m_language == QLatin1String("en")) {
        return sat ? QStringLiteral("Satellite") : QStringLiteral("Street map");
    }
    return sat ? QStringLiteral("卫星图") : QStringLiteral("天地图");
}
