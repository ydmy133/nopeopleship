#pragma once

#include "protocol/Telemetry.h"

#include <QObject>
#include <QString>
#include <limits>

class QTimer;

class TelemetryStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(double heading READ heading NOTIFY updated)
    Q_PROPERTY(double batteryLevel READ batteryLevel NOTIFY updated)
    Q_PROPERTY(double latitude READ latitude NOTIFY updated)
    Q_PROPERTY(double longitude READ longitude NOTIFY updated)
    Q_PROPERTY(double speedMs READ speedMs NOTIFY updated)
    Q_PROPERTY(double gnssYaw READ gnssYaw NOTIFY updated)
    Q_PROPERTY(double gyroZ READ gyroZ NOTIFY updated)
    Q_PROPERTY(int dataValid READ dataValid NOTIFY updated)
    Q_PROPERTY(int navReached READ navReached NOTIFY updated)
    Q_PROPERTY(double outputMoveSpeed READ outputMoveSpeed NOTIFY updated)
    Q_PROPERTY(double outputSteerSpeed READ outputSteerSpeed NOTIFY updated)
    Q_PROPERTY(double distError READ distError NOTIFY updated)
    Q_PROPERTY(double headingError READ headingError NOTIFY updated)
    Q_PROPERTY(double altitude READ altitude NOTIFY updated)
    Q_PROPERTY(double roll READ roll NOTIFY updated)
    Q_PROPERTY(double pitch READ pitch NOTIFY updated)
    Q_PROPERTY(QString latDisplay READ latDisplay NOTIFY updated)
    Q_PROPERTY(QString lonDisplay READ lonDisplay NOTIFY updated)
    Q_PROPERTY(QString latDir READ latDir NOTIFY updated)
    Q_PROPERTY(QString lonDir READ lonDir NOTIFY updated)
    Q_PROPERTY(QString sogDisplay READ sogDisplay NOTIFY updated)
    Q_PROPERTY(QString cogDisplay READ cogDisplay NOTIFY updated)
    Q_PROPERTY(QString hdgDisplay READ hdgDisplay NOTIFY updated)
    Q_PROPERTY(QString depthDisplay READ depthDisplay NOTIFY updated)
    Q_PROPERTY(QString depthTopLabel READ depthTopLabel NOTIFY updated)
    Q_PROPERTY(QString depthBottomLabel READ depthBottomLabel NOTIFY updated)
    Q_PROPERTY(qint64 ageMs READ ageMs NOTIFY onlineChanged)
    Q_PROPERTY(int parseErrors READ parseErrors NOTIFY statsChanged)
    Q_PROPERTY(int crcErrors READ crcErrors NOTIFY statsChanged)
    Q_PROPERTY(bool hasFix READ hasFix NOTIFY hasFixChanged)
    Q_PROPERTY(bool bdsConnected READ bdsConnected NOTIFY hasFixChanged)

public:
    explicit TelemetryStore(QObject *parent = nullptr);

    void ingest(const QByteArray &raw, const QString &channel);
    const Telemetry &latest() const { return m_tel; }
    bool hasFix() const;
    bool bdsConnected() const { return hasFix(); }
    bool isFresh(qint64 maxAgeMs) const;

    bool online() const { return m_online; }
    double heading() const { return m_tel.heading; }
    double batteryLevel() const { return m_tel.batteryLevel; }
    double latitude() const { return m_tel.rawLat; }
    double longitude() const { return m_tel.rawLon; }
    double speedMs() const { return m_tel.speedMs; }
    double gnssYaw() const { return m_tel.gnssYaw; }
    double gyroZ() const { return m_tel.gyroZ; }
    int dataValid() const { return m_tel.dataValid; }
    int navReached() const { return m_tel.navReached; }
    double outputMoveSpeed() const { return m_tel.outputMoveSpeed; }
    double outputSteerSpeed() const { return m_tel.outputSteerSpeed; }
    double distError() const { return m_tel.distError; }
    double headingError() const { return m_tel.headingError; }
    /** 协议已不上报海拔；保留属性供旧 QML 读到 NaN。 */
    double altitude() const { return std::numeric_limits<double>::quiet_NaN(); }
    double roll() const { return 0.0; }
    double pitch() const { return 0.0; }
    QString latDisplay() const { return m_latDisplay; }
    QString lonDisplay() const { return m_lonDisplay; }
    QString latDir() const { return m_latDir; }
    QString lonDir() const { return m_lonDir; }
    QString sogDisplay() const { return m_sogDisplay; }
    QString cogDisplay() const { return m_cogDisplay; }
    QString hdgDisplay() const { return m_hdgDisplay; }
    QString depthDisplay() const { return m_depthDisplay; }
    QString depthTopLabel() const { return m_depthTop; }
    QString depthBottomLabel() const { return m_depthBottom; }
    qint64 ageMs() const;
    int parseErrors() const { return m_parseErrors; }
    int crcErrors() const { return m_crcErrors; }

signals:
    void updated();
    void onlineChanged();
    void statsChanged();
    void hasFixChanged();

private:
    void refreshDisplays();
    void setOnline(bool on);
    void refreshFix();

    Telemetry m_tel;
    qint64 m_lastRxMs = 0;
    bool m_online = false;
    bool m_hasFix = false;
    int m_parseErrors = 0;
    int m_crcErrors = 0;
    QTimer *m_watchdog = nullptr;
    QString m_latDisplay = QStringLiteral("--");
    QString m_lonDisplay = QStringLiteral("--");
    QString m_latDir = QStringLiteral("N");
    QString m_lonDir = QStringLiteral("E");
    QString m_sogDisplay = QStringLiteral("--");
    QString m_cogDisplay = QStringLiteral("--");
    QString m_hdgDisplay = QStringLiteral("--");
    QString m_depthDisplay = QStringLiteral("--");
    QString m_depthTop = QStringLiteral("0.0m");
    QString m_depthBottom = QStringLiteral("-1.5m");
};
