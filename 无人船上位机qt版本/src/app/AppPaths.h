#pragma once

#include <QString>

namespace AppPaths {

void init();
void migrateLegacy();
QString dataDir();
QString settingsFile();
QString databaseFile();
QString logFile();
QString voyageLogFile();
bool usingInstallDir();

} // namespace AppPaths
