#include "app/UsvBridge.h"
#include "app/AppLog.h"
#include "app/AppPaths.h"
#include "control/ControlLoop.h"
#include "preset/PresetStore.h"
#include "preset/ReturnPointStore.h"
#include "settings/AppSettings.h"
#include "telemetry/TelemetryStore.h"
#include "map/MapController.h"
#include "map/TileImageProvider.h"

#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmapCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QStandardPaths>
#include <QtQml>

class CachingNam : public QNetworkAccessManager
{
public:
    explicit CachingNam(QObject *parent = nullptr)
        : QNetworkAccessManager(parent)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        QNetworkRequest req(request);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }
};

class MapNamFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        auto *nam = new CachingNam(parent);
        auto *cache = new QNetworkDiskCache(nam);
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                            + QStringLiteral("/map-tiles");
        QDir().mkpath(dir);
        cache->setCacheDirectory(dir);
        cache->setMaximumCacheSize(512LL * 1024 * 1024);
        nam->setCache(cache);
        return nam;
    }
};

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Zhudian"));
    app.setOrganizationDomain(QStringLiteral("zhudian.local"));
    app.setApplicationName(QStringLiteral("UsvGcs"));
    app.setApplicationDisplayName(QStringLiteral("无人船控制系统"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    AppPaths::init();
    AppLog::init();
    AppPaths::migrateLegacy();
    qInfo("==== 无人船控制系统启动 ====");
    qInfo("数据目录: %s", qUtf8Printable(AppPaths::dataDir()));
    qInfo("配置: %s", qUtf8Printable(AppPaths::settingsFile()));
    qInfo("数据库: %s", qUtf8Printable(AppPaths::databaseFile()));
    qInfo("日志: %s", qUtf8Printable(AppLog::primaryPath()));
    qInfo("航行日志: %s", qUtf8Printable(AppLog::voyagePath()));
    if (!AppPaths::usingInstallDir()) {
        qWarning("程序目录不可写，数据未放在 exe 旁的 data 文件夹");
    }

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(15);
    app.setFont(font);

    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile lock(lockDir + QStringLiteral("/usv-qt-gcs.lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(200)) {
        qWarning("another instance is running");
        return 0;
    }

    qmlRegisterUncreatableType<AppSettings>("Usv", 1, 0, "AppSettings", QStringLiteral("C++"));
    qmlRegisterUncreatableType<TelemetryStore>("Usv", 1, 0, "TelemetryStore", QStringLiteral("C++"));
    qmlRegisterUncreatableType<ControlLoop>("Usv", 1, 0, "ControlLoop", QStringLiteral("C++"));
    qmlRegisterUncreatableType<PresetStore>("Usv", 1, 0, "PresetStore", QStringLiteral("C++"));
    qmlRegisterUncreatableType<ReturnPointStore>("Usv", 1, 0, "ReturnPointStore", QStringLiteral("C++"));
    qmlRegisterUncreatableType<MapController>("Usv", 1, 0, "MapController", QStringLiteral("C++"));
    QPixmapCache::setCacheLimit(80 * 1024);

    UsvBridge bridge;

    MapNamFactory namFactory;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("usvtile"), new TileImageProvider(bridge.tileFetcher()));
    engine.setNetworkAccessManagerFactory(&namFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("usv"), &bridge);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [](QObject *obj, const QUrl &url) {
                         if (!obj) {
                             qCritical("Failed to create QML object: %s", qUtf8Printable(url.toString()));
                         }
                     },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("QML engine has no root objects");
        return 1;
    }

    return app.exec();
}
