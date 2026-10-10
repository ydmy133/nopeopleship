#include "app/AppLog.h"

#include "app/AppPaths.h"

#include <QFile>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QStringConverter>
#include <QTextStream>
#include <cstdio>

namespace {

QFile g_file;
QFile g_voyageFile;
QMutex g_mutex;
QMutex g_voyageMutex;

QString stamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"));
}

bool openAppend(QFile *file, const QString &path)
{
    file->setFileName(path);
    if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return true;
    }
    fprintf(stderr, "failed to open log: %s\n", qUtf8Printable(file->fileName()));
    return false;
}

void appendFile(const QString &line)
{
    if (!g_file.isOpen()) {
        return;
    }
    QTextStream ts(&g_file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << line;
    ts.flush();
}

void writeLine(const char *level, const QString &msg)
{
    const QString line = QStringLiteral("[%1] [%2] %3\n").arg(stamp(), QLatin1String(level), msg);
    fprintf(stderr, "%s", qUtf8Printable(line));
    QMutexLocker lock(&g_mutex);
    appendFile(line);
}

} // namespace

namespace AppLog {

void init()
{
    openAppend(&g_file, AppPaths::logFile());
    openAppend(&g_voyageFile, AppPaths::voyageLogFile());

    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
        const char *lv = "INFO";
        if (type == QtWarningMsg) {
            lv = "WARN";
        } else if (type == QtCriticalMsg || type == QtFatalMsg) {
            lv = "ERROR";
        } else if (type == QtDebugMsg) {
            lv = "DEBUG";
        }
        writeLine(lv, msg);
    });
}

QString localPath()
{
    return g_file.fileName();
}

QString appDataPath()
{
    return g_file.fileName();
}

QString primaryPath()
{
    return g_file.fileName();
}

QString voyagePath()
{
    return g_voyageFile.fileName();
}

void appendVoyage(const QByteArray &json)
{
    if (json.isEmpty()) {
        return;
    }
    QByteArray body = json;
    body.replace('\r', ' ');
    body.replace('\n', ' ');
    QByteArray line;
    line += '[';
    line += stamp().toUtf8();
    line += "] ";
    line += body;
    line += '\n';

    QMutexLocker lock(&g_voyageMutex);
    if (!g_voyageFile.isOpen()) {
        return;
    }
    g_voyageFile.write(line);
    g_voyageFile.flush();
}

void info(const QString &msg)
{
    qInfo("%s", qUtf8Printable(msg));
}

void warn(const QString &msg)
{
    qWarning("%s", qUtf8Printable(msg));
}

void error(const QString &msg)
{
    qCritical("%s", qUtf8Printable(msg));
}

} // namespace AppLog
