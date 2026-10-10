#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct PresetItem {
    int positionNumber = 0;
    QString name;
    double latitude = 0;
    double longitude = 0;
    QString address;
    bool hasCoord = false;
};

class PresetStore : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NumberRole = Qt::UserRole + 1,
        NameRole,
        LatitudeRole,
        LongitudeRole,
        AddressRole,
        HasCoordRole,
        LabelRole,
        SubtitleRole
    };

    explicit PresetStore(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool saveCoord(int number, double lat, double lon, const QString &address);
    bool clearCoord(int number);
    bool rename(int number, const QString &name);
    PresetItem itemAt(int number) const;

private:
    bool openDb();
    void ensureSlots();
    void reload();
    int rowForNumber(int number) const;
    void notifyRow(int row);

    QVector<PresetItem> m_items;
};
