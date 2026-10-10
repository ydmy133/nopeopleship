#include "app/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>

namespace {

QString g_dataDir;
bool g_usingInstallDir = false;

bool dirIsWritable(const QString &dir)
{
    const QString probe = dir + QStringLiteral("/.writable");
    QFile f(probe);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const bool ok = f.write("1") == 1;
    f.close();
    QFile::remove(probe);
    return ok;
}

QString tryMakeDataDir(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        return {};
    }
    if (!dirIsWritable(dir)) {
        return {};
    }
    return QDir::cleanPath(dir);
}

void migrateLegacyIfNeeded()
{
    const QString ini = AppPaths::settingsFile();
    if (!QFile::exists(ini)) {
        QSettings legacy;
        const QStringList keys = legacy.allKeys();
        if (!keys.isEmpty()) {
            QSettings dest(ini, QSettings::IniFormat);
            for (const QString &key : keys) {
                dest.setValue(key, legacy.value(key));
            }
            dest.sync();
            qInfo("[PATH] 已从注册表迁移配置到 %s", qUtf8Printable(ini));
        }
    }

    const QString db = AppPaths::databaseFile();
    if (QFile::exists(db)) {
        return;
    }
    const QString oldDb = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + QStringLiteral("/usvdb.sqlite");
    if (QFileInfo::exists(oldDb) && QDir::cleanPath(oldDb) != QDir::cleanPath(db)) {
        if (QFile::copy(oldDb, db)) {
            qInfo("[PATH] 已复制预置位库到 %s", qUtf8Printable(db));
        } else {
            qWarning("[PATH] 复制预置位库失败：%s -> %s",
                     qUtf8Printable(oldDb), qUtf8Printable(db));
        }
    }
}

} // namespace

namespace AppPaths {

void init()
{
    const QString preferred = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    g_dataDir = tryMakeDataDir(preferred);
    g_usingInstallDir = !g_dataDir.isEmpty();
    if (g_dataDir.isEmpty()) {
        const QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        g_dataDir = tryMakeDataDir(fallback);
        g_usingInstallDir = false;
        qWarning("[PATH] 程序目录不可写，数据改放到 %s", qUtf8Printable(g_dataDir));
    }
    if (g_dataDir.isEmpty()) {
        g_dataDir = QDir::tempPath() + QStringLiteral("/UsvGcs");
        QDir().mkpath(g_dataDir);
        qWarning("[PATH] 使用临时目录存放数据：%s", qUtf8Printable(g_dataDir));
    }
}

void migrateLegacy()
{
    migrateLegacyIfNeeded();
}

QString dataDir()
{
    return g_dataDir;
}

QString settingsFile()
{
    return g_dataDir + QStringLiteral("/usv_gcs.ini");
}

QString databaseFile()
{
    return g_dataDir + QStringLiteral("/usvdb.sqlite");
}

QString logFile()
{
    return g_dataDir + QStringLiteral("/usv_gcs.log");
}

QString voyageLogFile()
{
    return g_dataDir + QStringLiteral("/usv_voyage.log");
}

bool usingInstallDir()
{
    return g_usingInstallDir;
}

} // namespace AppPaths
