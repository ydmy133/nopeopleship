#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class AppSettings;
class TelemetryStore;
class QTimer;

class MapController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(double centerLat READ centerLat NOTIFY viewChanged)
    Q_PROPERTY(double centerLon READ centerLon NOTIFY viewChanged)
    Q_PROPERTY(int zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(int tileSize READ tileSize CONSTANT)
    Q_PROPERTY(bool boatVisible READ boatVisible NOTIFY viewChanged)
    Q_PROPERTY(double heading READ heading NOTIFY viewChanged)
    Q_PROPERTY(double layerX READ layerX NOTIFY layerMoved)
    Q_PROPERTY(double layerY READ layerY NOTIFY layerMoved)
    Q_PROPERTY(double originX READ originX NOTIFY originChanged)
    Q_PROPERTY(double originY READ originY NOTIFY originChanged)

public:
    enum Roles {
        TxRole = Qt::UserRole + 1,
        TyRole,
        ZRole,
        ImageUrlRole,
        OverlayUrlRole,
        TileKeyRole
    };

    explicit MapController(AppSettings *settings, TelemetryStore *telemetry, QObject *parent = nullptr);

    double centerLat() const { return m_centerLat; }
    double centerLon() const { return m_centerLon; }
    int zoom() const { return m_zoom; }
    void setZoom(int z);
    int tileSize() const { return kTileSize; }
    bool boatVisible() const { return m_boatVisible; }
    double heading() const { return m_heading; }
    double originX() const { return m_originX; }
    double originY() const { return m_originY; }
    double layerX() const { return -(m_worldX - m_originX) + m_viewW / 2.0; }
    double layerY() const { return -(m_worldY - m_originY) + m_viewH / 2.0; }

    Q_INVOKABLE void setViewport(int width, int height);
    Q_INVOKABLE void zoomBy(int delta);
    Q_INVOKABLE void panBy(double dx, double dy);
    Q_INVOKABLE void beginPan();
    Q_INVOKABLE void endPan();
    void reloadTiles();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void viewChanged();
    void layerMoved();
    void originChanged();

private:
    struct Tile {
        int x = 0;
        int tx = 0;
        int y = 0;
        int z = 0;
        QString imageUrl;
        QString overlayUrl;
        QString key;
    };

    static constexpr int kTileSize = 256;
    static constexpr int kMinZoom = 3;
    static constexpr int kMaxNativeZoom = 14;
    static constexpr int kMaxZoom = 18;
    static constexpr double kGuangzhouLat = 23.1291;
    static constexpr double kGuangzhouLon = 113.2644;
    static constexpr int kDefaultZoom = 12;

    void onTelemetry();
    void persistViewLater();
    void persistViewNow();
    void syncWorldFromCenter();
    void syncCenterFromWorld();
    void scheduleRebuild();
    void rebuildTiles();
    int nativeZoom() const;
    bool tileVisible(const Tile &t) const;
    void appendLevel(QVector<Tile> &out, int tz, int pad, bool withOverlay, int imagePrio) const;
    void sortTilesForLoad(QVector<Tile> &tiles) const;
    double tileCenterDist2(const Tile &t) const;
    QString tileUrl(int prio, const QString &layer, int z, int x, int y) const;
    QString mapKind() const;

    AppSettings *m_settings = nullptr;
    TelemetryStore *m_telemetry = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    QVector<Tile> m_tiles;
    double m_centerLat = kGuangzhouLat;
    double m_centerLon = kGuangzhouLon;
    double m_heading = 0;
    int m_zoom = kDefaultZoom;
    int m_viewW = 0;
    int m_viewH = 0;
    bool m_boatVisible = false;
    bool m_followPaused = false;
    double m_worldX = 0;
    double m_worldY = 0;
    double m_originX = 0;
    double m_originY = 0;
};
