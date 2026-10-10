#include "preset/ReturnPointStore.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

ReturnPointStore::ReturnPointStore(QObject *parent)
    : QAbstractListModel(parent)
{
    if (ensureTable()) {
        reload();
    }
}

bool ReturnPointStore::ensureTable()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("usv"));
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "return_point db not open";
        return false;
    }
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS return_point ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT,"
            "longitude REAL,"
            "latitude REAL,"
            "address TEXT,"
            "create_time TEXT,"
            "update_time TEXT)"))) {
        qWarning() << "return_point create failed" << q.lastError().text();
        return false;
    }
    return true;
}

void ReturnPointStore::reload()
{
    beginResetModel();
    m_items.clear();
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.exec(QStringLiteral(
        "SELECT id,name,latitude,longitude,address FROM return_point ORDER BY id"));
    while (q.next()) {
        ReturnPoint it;
        it.id = q.value(0).toInt();
        it.name = q.value(1).toString();
        it.latitude = q.value(2).toDouble();
        it.longitude = q.value(3).toDouble();
        it.address = q.value(4).toString();
        m_items.push_back(it);
    }
    endResetModel();
    emit countChanged();
}

int ReturnPointStore::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant ReturnPointStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const ReturnPoint &it = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return it.id;
    case NameRole:
        return it.name;
    case LatitudeRole:
        return it.latitude;
    case LongitudeRole:
        return it.longitude;
    case AddressRole:
        return it.address;
    case LabelRole:
        return it.name.isEmpty() ? QStringLiteral("返航点#%1").arg(it.id) : it.name;
    case SubtitleRole:
        return QStringLiteral("%1, %2").arg(it.latitude, 0, 'f', 6).arg(it.longitude, 0, 'f', 6);
    default:
        return {};
    }
}

QHash<int, QByteArray> ReturnPointStore::roleNames() const
{
    return {
        {IdRole, "pointId"},
        {NameRole, "name"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {AddressRole, "address"},
        {LabelRole, "label"},
        {SubtitleRole, "subtitle"},
    };
}

QString ReturnPointStore::nextDefaultName() const
{
    int maxN = 0;
    for (const auto &it : m_items) {
        const QString prefix = QStringLiteral("返航点#");
        if (it.name.startsWith(prefix)) {
            bool ok = false;
            const int n = it.name.mid(prefix.size()).toInt(&ok);
            if (ok) {
                maxN = std::max(maxN, n);
            }
        }
    }
    return QStringLiteral("返航点#%1").arg(maxN + 1);
}

bool ReturnPointStore::addPoint(const QString &name, double lat, double lon, const QString &address, int *idOut)
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral(
        "INSERT INTO return_point(name,latitude,longitude,address,create_time,update_time) "
        "VALUES(?,?,?,?,?,?)"));
    q.addBindValue(name);
    q.addBindValue(lat);
    q.addBindValue(lon);
    q.addBindValue(address);
    q.addBindValue(now);
    q.addBindValue(now);
    if (!q.exec()) {
        qWarning() << "return_point insert failed" << q.lastError().text();
        return false;
    }
    ReturnPoint it;
    it.id = q.lastInsertId().toInt();
    it.name = name;
    it.latitude = lat;
    it.longitude = lon;
    it.address = address;
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(it);
    endInsertRows();
    emit countChanged();
    if (idOut) {
        *idOut = it.id;
    }
    return true;
}

bool ReturnPointStore::updateCoord(int id, double lat, double lon, const QString &address)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral(
        "UPDATE return_point SET latitude=?, longitude=?, address=?, update_time=? WHERE id=?"));
    q.addBindValue(lat);
    q.addBindValue(lon);
    q.addBindValue(address);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForId(id);
    if (row >= 0) {
        m_items[row].latitude = lat;
        m_items[row].longitude = lon;
        m_items[row].address = address;
        notifyRow(row);
    }
    return true;
}

bool ReturnPointStore::removePoint(int id)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral("DELETE FROM return_point WHERE id=?"));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForId(id);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
        emit countChanged();
    }
    return true;
}

bool ReturnPointStore::rename(int id, const QString &name)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral("UPDATE return_point SET name=?, update_time=? WHERE id=?"));
    q.addBindValue(name);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForId(id);
    if (row >= 0) {
        m_items[row].name = name;
        notifyRow(row);
    }
    return true;
}

int ReturnPointStore::rowForId(int id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void ReturnPointStore::notifyRow(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

ReturnPoint ReturnPointStore::itemAt(int id) const
{
    for (const auto &it : m_items) {
        if (it.id == id) {
            return it;
        }
    }
    return {};
}
