#pragma once

#include <QByteArray>
#include <QString>

namespace AppLog {

void init();
QString localPath();
QString appDataPath();
QString primaryPath();
QString voyagePath();
void appendVoyage(const QByteArray &json);
void info(const QString &msg);
void warn(const QString &msg);
void error(const QString &msg);

} // namespace AppLog
