#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct ReturnPoint {
    int id = 0;
    QString name;
    double latitude = 0;
    double longitude = 0;
    QString address;
};

class ReturnPointStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        LatitudeRole,
        LongitudeRole,
        AddressRole,
        LabelRole,
        SubtitleRole
    };

    explicit ReturnPointStore(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool addPoint(const QString &name, double lat, double lon, const QString &address, int *idOut = nullptr);
    bool updateCoord(int id, double lat, double lon, const QString &address);
    bool removePoint(int id);
    bool rename(int id, const QString &name);
    ReturnPoint itemAt(int id) const;
    QString nextDefaultName() const;
    int count() const { return m_items.size(); }

signals:
    void countChanged();

private:
    bool ensureTable();
    void reload();
    int rowForId(int id) const;
    void notifyRow(int row);

    QVector<ReturnPoint> m_items;
};
