#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QTimer;

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString commMode READ commMode WRITE setCommMode NOTIFY commModeChanged)
    Q_PROPERTY(QString mqttHost READ mqttHost WRITE setMqttHost NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttScheme READ mqttScheme WRITE setMqttScheme NOTIFY mqttChanged)
    Q_PROPERTY(int mqttPort READ mqttPort WRITE setMqttPort NOTIFY mqttChanged)
    Q_PROPERTY(bool mqttSsl READ mqttSsl WRITE setMqttSsl NOTIFY mqttChanged)
    Q_PROPERTY(bool mqttSslInsecure READ mqttSslInsecure WRITE setMqttSslInsecure NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttUsername READ mqttUsername WRITE setMqttUsername NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttPassword READ mqttPassword WRITE setMqttPassword NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttClientId READ mqttClientId WRITE setMqttClientId NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttBoatId READ mqttBoatId WRITE setMqttBoatId NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttPublishTopic READ mqttPublishTopic WRITE setMqttPublishTopic NOTIFY mqttChanged)
    Q_PROPERTY(QString mqttTelemetryTopic READ mqttTelemetryTopic WRITE setMqttTelemetryTopic NOTIFY mqttChanged)
    Q_PROPERTY(QString udpLocalHost READ udpLocalHost WRITE setUdpLocalHost NOTIFY udpChanged)
    Q_PROPERTY(int udpLocalPort READ udpLocalPort WRITE setUdpLocalPort NOTIFY udpChanged)
    Q_PROPERTY(QString udpRemoteHost READ udpRemoteHost WRITE setUdpRemoteHost NOTIFY udpChanged)
    Q_PROPERTY(int udpRemotePort READ udpRemotePort WRITE setUdpRemotePort NOTIFY udpChanged)
    Q_PROPERTY(bool dcProtOn READ dcProtOn WRITE setDcProtOn NOTIFY protectionChanged)
    Q_PROPERTY(bool lbProtOn READ lbProtOn WRITE setLbProtOn NOTIFY protectionChanged)
    Q_PROPERTY(double returnLat READ returnLat WRITE setReturnLat NOTIFY returnChanged)
    Q_PROPERTY(double returnLon READ returnLon WRITE setReturnLon NOTIFY returnChanged)
    Q_PROPERTY(bool hasReturnPoint READ hasReturnPoint NOTIFY returnChanged)
    Q_PROPERTY(double oneKeyLat READ oneKeyLat WRITE setOneKeyLat NOTIFY oneKeyChanged)
    Q_PROPERTY(double oneKeyLon READ oneKeyLon WRITE setOneKeyLon NOTIFY oneKeyChanged)
    Q_PROPERTY(bool hasOneKeyPoint READ hasOneKeyPoint NOTIFY oneKeyChanged)
    Q_PROPERTY(int activeReturnId READ activeReturnId NOTIFY oneKeyChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY uiChanged)
    Q_PROPERTY(QString languageName READ languageName NOTIFY uiChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY uiChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY uiChanged)
    Q_PROPERTY(QString displayUnit READ displayUnit WRITE setDisplayUnit NOTIFY uiChanged)
    Q_PROPERTY(QString displayUnitName READ displayUnitName NOTIFY uiChanged)
    Q_PROPERTY(QString dcpa READ dcpa WRITE setDcpa NOTIFY uiChanged)
    Q_PROPERTY(QString tcpa READ tcpa WRITE setTcpa NOTIFY uiChanged)
    Q_PROPERTY(bool alertVoice READ alertVoice WRITE setAlertVoice NOTIFY uiChanged)
    Q_PROPERTY(bool flash READ flash WRITE setFlash NOTIFY uiChanged)
    Q_PROPERTY(bool aisLabel READ aisLabel WRITE setAisLabel NOTIFY uiChanged)
    Q_PROPERTY(int moveSpeedPercent READ moveSpeedPercent WRITE setMoveSpeedPercent NOTIFY motorChanged)
    Q_PROPERTY(int steerSpeedPercent READ steerSpeedPercent WRITE setSteerSpeedPercent NOTIFY motorChanged)
    Q_PROPERTY(int speedPercent READ speedPercent NOTIFY motorChanged)
    Q_PROPERTY(QString mapType READ mapType WRITE setMapType NOTIFY mapChanged)
    Q_PROPERTY(QString mapBaseUrl READ mapBaseUrl WRITE setMapBaseUrl NOTIFY mapChanged)
    Q_PROPERTY(double mapLastLat READ mapLastLat NOTIFY mapChanged)
    Q_PROPERTY(double mapLastLon READ mapLastLon NOTIFY mapChanged)
    Q_PROPERTY(int mapLastZoom READ mapLastZoom NOTIFY mapChanged)
    Q_PROPERTY(bool hasMapLastView READ hasMapLastView NOTIFY mapChanged)

public:
    static constexpr int kDefaultSpeedPercent = 20;
    explicit AppSettings(QObject *parent = nullptr);
    ~AppSettings() override;

    QString commMode() const { return m_commMode; }
    void setCommMode(const QString &v);

    QString mqttHost() const { return m_mqttHost; }
    void setMqttHost(const QString &v);
    QString mqttScheme() const { return m_mqttScheme; }
    void setMqttScheme(const QString &v);
    bool mqttUsesWebSocket() const;
    int mqttPort() const { return m_mqttPort; }
    void setMqttPort(int v);
    bool mqttSsl() const { return m_mqttSsl; }
    void setMqttSsl(bool v);
    bool mqttSslInsecure() const { return m_mqttSslInsecure; }
    void setMqttSslInsecure(bool v);
    QString mqttUsername() const { return m_mqttUsername; }
    void setMqttUsername(const QString &v);
    QString mqttPassword() const { return m_mqttPassword; }
    void setMqttPassword(const QString &v);
    QString mqttClientId() const { return m_mqttClientId; }
    void setMqttClientId(const QString &v);
    QString mqttBoatId() const { return m_mqttBoatId; }
    void setMqttBoatId(const QString &v);
    QString mqttPublishTopic() const { return m_mqttPublishTopic; }
    void setMqttPublishTopic(const QString &v);
    QString mqttTelemetryTopic() const { return m_mqttTelemetryTopic; }
    void setMqttTelemetryTopic(const QString &v);

    QString udpLocalHost() const { return m_udpLocalHost; }
    void setUdpLocalHost(const QString &v);
    int udpLocalPort() const { return m_udpLocalPort; }
    void setUdpLocalPort(int v);
    QString udpRemoteHost() const { return m_udpRemoteHost; }
    void setUdpRemoteHost(const QString &v);
    int udpRemotePort() const { return m_udpRemotePort; }
    void setUdpRemotePort(int v);

    bool dcProtOn() const { return m_dcProtOn; }
    void setDcProtOn(bool v);
    bool lbProtOn() const { return m_lbProtOn; }
    void setLbProtOn(bool v);

    double returnLat() const { return m_returnLat; }
    void setReturnLat(double v);
    double returnLon() const { return m_returnLon; }
    void setReturnLon(double v);
    bool hasReturnPoint() const { return m_hasReturn; }
    Q_INVOKABLE void clearReturnPoint();
    void setReturnPoint(double lat, double lon);

    double oneKeyLat() const { return m_oneKeyLat; }
    void setOneKeyLat(double v);
    double oneKeyLon() const { return m_oneKeyLon; }
    void setOneKeyLon(double v);
    bool hasOneKeyPoint() const { return m_hasOneKey; }
    int activeReturnId() const { return m_activeReturnId; }
    void setActiveReturnId(int id);
    void setOneKeyPoint(double lat, double lon, int returnId = -1);

    QString language() const { return m_language; }
    void setLanguage(const QString &v);
    QString theme() const { return m_theme; }
    void setTheme(const QString &v);
    QString displayUnit() const { return m_displayUnit; }
    void setDisplayUnit(const QString &v);
    QString dcpa() const { return m_dcpa; }
    void setDcpa(const QString &v);
    QString tcpa() const { return m_tcpa; }
    void setTcpa(const QString &v);
    bool alertVoice() const { return m_alertVoice; }
    void setAlertVoice(bool v);
    bool flash() const { return m_flash; }
    void setFlash(bool v);
    bool aisLabel() const { return m_aisLabel; }
    void setAisLabel(bool v);

    int moveSpeedPercent() const { return m_moveSpeedPercent; }
    void setMoveSpeedPercent(int v);
    int steerSpeedPercent() const { return m_steerSpeedPercent; }
    void setSteerSpeedPercent(int v);
    Q_INVOKABLE int normalizeSpeedPercent(int v) const;
    Q_INVOKABLE int speedPercent() const;
    Q_INVOKABLE int moveCommandSpeed() const;
    Q_INVOKABLE int steerCommandSpeed() const;

    Q_INVOKABLE void cycleLanguage();
    Q_INVOKABLE void cycleTheme();
    Q_INVOKABLE void cycleDisplayUnit();
    Q_INVOKABLE QString displayUnitName() const;
    Q_INVOKABLE QString languageName() const;
    Q_INVOKABLE QString themeName() const;
    Q_INVOKABLE QString mapTypeName() const;
    Q_INVOKABLE void cycleMapType();

    QString mapType() const { return m_mapType; }
    void setMapType(const QString &v);
    QString mapBaseUrl() const { return m_mapBaseUrl; }
    void setMapBaseUrl(const QString &v);
    double mapLastLat() const { return m_mapLastLat; }
    double mapLastLon() const { return m_mapLastLon; }
    int mapLastZoom() const { return m_mapLastZoom; }
    bool hasMapLastView() const { return m_hasMapLast; }
    void setMapLastView(double lat, double lon, int zoom);

    bool isMqttMode() const { return m_commMode == QLatin1String("mqtt"); }

signals:
    void commModeChanged();
    void mqttChanged();
    void udpChanged();
    void protectionChanged();
    void returnChanged();
    void oneKeyChanged();
    void uiChanged();
    void motorChanged();
    void mapChanged();

private:
    void load();
    void save();
    void saveNow();
    int commandSpeedFromPercent(int pct) const;

    QTimer *m_saveTimer = nullptr;

    QString m_commMode = QStringLiteral("mqtt");
    QString m_mqttHost = QStringLiteral("42.193.140.103");
    QString m_mqttScheme = QStringLiteral("mqtt");
    int m_mqttPort = 1883;
    bool m_mqttSsl = false;
    bool m_mqttSslInsecure = false;
    QString m_mqttUsername = QStringLiteral("java_backend");
    QString m_mqttPassword;
    QString m_mqttClientId;
    QString m_mqttBoatId = QStringLiteral("usv_001");
    QString m_mqttPublishTopic = QStringLiteral("usv/usv_001/cmd/control");
    QString m_mqttTelemetryTopic = QStringLiteral("usv/+/telemetry/#");
    QString m_udpLocalHost = QStringLiteral("192.168.1.81");
    int m_udpLocalPort = 10193;
    QString m_udpRemoteHost = QStringLiteral("192.168.1.80");
    int m_udpRemotePort = 10123;
    bool m_dcProtOn = true;
    bool m_lbProtOn = true;
    double m_returnLat = 0;
    double m_returnLon = 0;
    bool m_hasReturn = false;
    double m_oneKeyLat = 0;
    double m_oneKeyLon = 0;
    bool m_hasOneKey = false;
    int m_activeReturnId = -1;
    QString m_language = QStringLiteral("zh");
    QString m_theme = QStringLiteral("day");
    QString m_displayUnit = QStringLiteral("nm");
    QString m_dcpa = QStringLiteral("0.5NM");
    QString m_tcpa = QStringLiteral("20min");
    bool m_alertVoice = true;
    bool m_flash = true;
    bool m_aisLabel = true;
    int m_moveSpeedPercent = kDefaultSpeedPercent;
    int m_steerSpeedPercent = kDefaultSpeedPercent;
    QString m_mapType = QStringLiteral("street");
    QString m_mapBaseUrl = QStringLiteral("http://210.76.81.182");
    double m_mapLastLat = 23.1291;
    double m_mapLastLon = 113.2644;
    int m_mapLastZoom = 12;
    bool m_hasMapLast = false;
};
