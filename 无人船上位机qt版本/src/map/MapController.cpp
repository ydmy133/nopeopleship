#include "map/MapController.h"

#include "protocol/Geo.h"
#include "settings/AppSettings.h"
#include "telemetry/TelemetryStore.h"

#include <QSet>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstdlib>

MapController::MapController(AppSettings *settings, TelemetryStore *telemetry, QObject *parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
    , m_telemetry(telemetry)
{
    if (m_settings && m_settings->hasMapLastView()) {
        m_centerLat = m_settings->mapLastLat();
        m_centerLon = m_settings->mapLastLon();
        m_zoom = Geo::clampInt(m_settings->mapLastZoom(), kMinZoom, kMaxZoom);
    }
    syncWorldFromCenter();
    m_originX = m_worldX;
    m_originY = m_worldY;

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer, &QTimer::timeout, this, &MapController::persistViewNow);

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(80);
    connect(m_rebuildTimer, &QTimer::timeout, this, &MapController::rebuildTiles);

    if (m_telemetry) {
        connect(m_telemetry, &TelemetryStore::updated, this, &MapController::onTelemetry);
        connect(m_telemetry, &TelemetryStore::hasFixChanged, this, &MapController::onTelemetry);
        onTelemetry();
    }
}

void MapController::reloadTiles()
{
    if (m_rebuildTimer) {
        m_rebuildTimer->stop();
    }
    beginResetModel();
    m_tiles.clear();
    endResetModel();
    rebuildTiles();
}

void MapController::setZoom(int z)
{
    z = Geo::clampInt(z, kMinZoom, kMaxZoom);
    if (m_zoom == z) {
        return;
    }
    const int oldZ = m_zoom;
    m_zoom = z;
    m_worldX *= std::ldexp(1.0, z - oldZ);
    m_worldY *= std::ldexp(1.0, z - oldZ);
    m_centerLon = Geo::clampDouble(Geo::tileXToLon(m_worldX / kTileSize, m_zoom), -180.0, 180.0);
    m_centerLat = Geo::clampDouble(Geo::tileYToLat(m_worldY / kTileSize, m_zoom), -85.05112878, 85.05112878);
    emit viewChanged();
    rebuildTiles();
    persistViewLater();
}

void MapController::zoomBy(int delta)
{
    if (delta == 0) {
        return;
    }
    setZoom(m_zoom + delta);
}

void MapController::setViewport(int width, int height)
{
    width = std::max(0, width);
    height = std::max(0, height);
    if (m_viewW == width && m_viewH == height) {
        return;
    }
    m_viewW = width;
    m_viewH = height;
    emit layerMoved();
    rebuildTiles();
}

void MapController::onTelemetry()
{
    if (!m_telemetry) {
        return;
    }
    const bool fix = m_telemetry->hasFix();
    bool centerDirty = false;
    bool viewDirty = false;
    if (fix && !m_followPaused) {
        const double lat = m_telemetry->latitude();
        const double lon = m_telemetry->longitude();
        if (!qFuzzyCompare(m_centerLat + 1.0, lat + 1.0) || !qFuzzyCompare(m_centerLon + 1.0, lon + 1.0)) {
            m_centerLat = Geo::clampDouble(lat, -85.05112878, 85.05112878);
            m_centerLon = Geo::clampDouble(lon, -180.0, 180.0);
            centerDirty = true;
            viewDirty = true;
        }
        persistViewLater();
    }
    const double hdg = m_telemetry->heading();
    if (!qFuzzyCompare(m_heading + 1.0, hdg + 1.0)) {
        m_heading = hdg;
        viewDirty = true;
    }
    if (m_boatVisible != fix) {
        m_boatVisible = fix;
        viewDirty = true;
    }
    if (centerDirty) {
        syncWorldFromCenter();
        emit layerMoved();
        scheduleRebuild();
        return;
    }
    if (viewDirty) {
        emit viewChanged();
    }
}

void MapController::persistViewLater()
{
    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void MapController::persistViewNow()
{
    if (m_settings) {
        m_settings->setMapLastView(m_centerLat, m_centerLon, m_zoom);
    }
}

void MapController::syncWorldFromCenter()
{
    m_worldX = Geo::lonToTileX(m_centerLon, m_zoom) * kTileSize;
    m_worldY = Geo::latToTileY(m_centerLat, m_zoom) * kTileSize;
}

void MapController::syncCenterFromWorld()
{
    m_centerLon = Geo::clampDouble(Geo::tileXToLon(m_worldX / kTileSize, m_zoom), -180.0, 180.0);
    m_centerLat = Geo::clampDouble(Geo::tileYToLat(m_worldY / kTileSize, m_zoom), -85.05112878, 85.05112878);
}

void MapController::scheduleRebuild()
{
    const double dist = std::max(std::abs(m_worldX - m_originX), std::abs(m_worldY - m_originY));
    if (dist >= static_cast<double>(kTileSize)) {
        if (m_rebuildTimer) {
            m_rebuildTimer->stop();
        }
        rebuildTiles();
        return;
    }
    if (m_rebuildTimer) {
        m_rebuildTimer->start();
    }
}

void MapController::beginPan()
{
    m_followPaused = true;
}

void MapController::endPan()
{
    m_followPaused = false;
    if (m_rebuildTimer) {
        m_rebuildTimer->stop();
    }
    rebuildTiles();
    persistViewLater();
    if (m_telemetry && m_telemetry->hasFix()) {
        onTelemetry();
    }
}

void MapController::panBy(double dx, double dy)
{
    if (dx == 0.0 && dy == 0.0) {
        return;
    }
    m_followPaused = true;
    m_worldX -= dx;
    m_worldY -= dy;
    emit layerMoved();
    scheduleRebuild();
}

QString MapController::mapKind() const
{
    if (m_settings && m_settings->mapType() == QLatin1String("satellite")) {
        return QStringLiteral("satellite");
    }
    return QStringLiteral("street");
}

QString MapController::tileUrl(int prio, const QString &layer, int z, int x, int y) const
{
    return QStringLiteral("image://usvtile/%1/%2/%3/%4/%5/%6")
        .arg(prio)
        .arg(mapKind())
        .arg(layer)
        .arg(z)
        .arg(x)
        .arg(y);
}

double MapController::tileCenterDist2(const Tile &t) const
{
    const double scale = std::ldexp(1.0, m_zoom - t.z);
    const double tilePx = static_cast<double>(kTileSize) * scale;
    const double mx = (static_cast<double>(t.tx) + 0.5) * tilePx;
    const double my = (static_cast<double>(t.y) + 0.5) * tilePx;
    const double dx = mx - m_worldX;
    const double dy = my - m_worldY;
    return dx * dx + dy * dy;
}

int MapController::nativeZoom() const
{
    return std::min(m_zoom, kMaxNativeZoom);
}

bool MapController::tileVisible(const Tile &t) const
{
    const double scale = std::ldexp(1.0, m_zoom - t.z);
    const double tilePx = static_cast<double>(kTileSize) * scale;
    const double px = static_cast<double>(t.tx) * tilePx;
    const double py = static_cast<double>(t.y) * tilePx;
    const double pad = tilePx;
    const double left = m_worldX - m_viewW / 2.0 - pad;
    const double top = m_worldY - m_viewH / 2.0 - pad;
    const double right = m_worldX + m_viewW / 2.0 + pad;
    const double bottom = m_worldY + m_viewH / 2.0 + pad;
    return px + tilePx >= left && px <= right && py + tilePx >= top && py <= bottom;
}

void MapController::appendLevel(QVector<Tile> &out, int tz, int pad, bool withOverlay, int imagePrio) const
{
    tz = Geo::clampInt(tz, kMinZoom, kMaxNativeZoom);
    const int n = 1 << tz;
    const double scale = std::ldexp(1.0, m_zoom - tz);
    const double tilePx = static_cast<double>(kTileSize) * scale;
    const int minTx = static_cast<int>(std::floor((m_worldX - m_viewW / 2.0) / tilePx)) - pad;
    const int maxTx = static_cast<int>(std::floor((m_worldX + m_viewW / 2.0) / tilePx)) + pad;
    const int minTy = std::max(0, static_cast<int>(std::floor((m_worldY - m_viewH / 2.0) / tilePx)) - pad);
    const int maxTy = std::min(n - 1, static_cast<int>(std::floor((m_worldY + m_viewH / 2.0) / tilePx)) + pad);

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            int wrappedX = tx % n;
            if (wrappedX < 0) {
                wrappedX += n;
            }
            Tile t;
            t.x = wrappedX;
            t.tx = tx;
            t.y = ty;
            t.z = tz;
            t.imageUrl = tileUrl(imagePrio, QStringLiteral("image"), tz, wrappedX, ty);
            if (withOverlay) {
                t.overlayUrl = tileUrl(2, QStringLiteral("network"), tz, wrappedX, ty);
            }
            t.key = QStringLiteral("%1/%2/%3/%4").arg(mapKind()).arg(tz).arg(tx).arg(ty);
            out.push_back(t);
        }
    }
}

void MapController::sortTilesForLoad(QVector<Tile> &tiles) const
{
    std::sort(tiles.begin(), tiles.end(), [this](const Tile &a, const Tile &b) {
        if (a.z != b.z) {
            return a.z < b.z;
        }
        return tileCenterDist2(a) < tileCenterDist2(b);
    });
}

void MapController::rebuildTiles()
{
    if (m_viewW <= 0 || m_viewH <= 0) {
        return;
    }

    syncCenterFromWorld();
    if (m_originX != m_worldX || m_originY != m_worldY) {
        m_originX = m_worldX;
        m_originY = m_worldY;
        emit originChanged();
        emit layerMoved();
    }

    const int nz = nativeZoom();
    QVector<Tile> needed;
    needed.reserve(160);
    appendLevel(needed, nz, 2, true, 1);
    if (m_tiles.isEmpty() && nz >= kMinZoom + 2) {
        appendLevel(needed, nz - 2, 0, false, 0);
    }

    QSet<QString> keepKeys;
    keepKeys.reserve(needed.size() + m_tiles.size());
    for (const Tile &t : needed) {
        keepKeys.insert(t.key);
    }
    const QString kindPrefix = mapKind() + QLatin1Char('/');
    for (const Tile &old : m_tiles) {
        if (keepKeys.contains(old.key)) {
            continue;
        }
        if (!old.key.startsWith(kindPrefix)) {
            continue;
        }
        if (old.z > kMaxNativeZoom || old.z < nz - 4) {
            continue;
        }
        if (!tileVisible(old)) {
            continue;
        }
        keepKeys.insert(old.key);
    }

    for (int i = m_tiles.size() - 1; i >= 0; --i) {
        if (!keepKeys.contains(m_tiles[i].key)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_tiles.removeAt(i);
            endRemoveRows();
        }
    }

    QSet<QString> have;
    have.reserve(m_tiles.size());
    for (const Tile &t : m_tiles) {
        have.insert(t.key);
    }

    QVector<Tile> toAdd;
    toAdd.reserve(needed.size());
    for (const Tile &t : needed) {
        if (!have.contains(t.key)) {
            toAdd.push_back(t);
        }
    }
    sortTilesForLoad(toAdd);
    if (!toAdd.isEmpty()) {
        const int row = m_tiles.size();
        beginInsertRows(QModelIndex(), row, row + toAdd.size() - 1);
        m_tiles.append(toAdd);
        endInsertRows();
    }
}

int MapController::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tiles.size();
}

QVariant MapController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tiles.size()) {
        return {};
    }
    const Tile &t = m_tiles.at(index.row());
    switch (role) {
    case TxRole:
        return t.tx;
    case TyRole:
        return t.y;
    case ZRole:
        return t.z;
    case ImageUrlRole:
        return t.imageUrl;
    case OverlayUrlRole:
        return t.overlayUrl;
    case TileKeyRole:
        return t.key;
    default:
        return {};
    }
}

QHash<int, QByteArray> MapController::roleNames() const
{
    return {
        {TxRole, "tx"},
        {TyRole, "ty"},
        {ZRole, "tz"},
        {ImageUrlRole, "imageUrl"},
        {OverlayUrlRole, "overlayUrl"},
        {TileKeyRole, "tileKey"},
    };
}
