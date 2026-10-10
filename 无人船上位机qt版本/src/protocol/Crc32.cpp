#include "protocol/Crc32.h"

namespace Crc32 {

quint32 ieee(const QByteArray &data)
{
    static quint32 table[256];
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    const auto *bytes = reinterpret_cast<const uchar *>(data.constData());
    const int n = data.size();
    for (int i = 0; i < n; ++i) {
        crc = table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace Crc32
