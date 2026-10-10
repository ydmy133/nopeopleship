#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace Crc32 {
quint32 ieee(const QByteArray &data);
}
