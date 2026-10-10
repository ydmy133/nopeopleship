#include "preset/PresetStore.h"

#include "app/AppPaths.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

PresetStore::PresetStore(QObject *parent)
    : QAbstractListModel(parent)
{
    if (openDb()) {
        ensureSlots();
        reload();
    }
}

bool PresetStore::openDb()
{
    const QString path = AppPaths::databaseFile();
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("usv"));
    db.setDatabaseName(path);
    qInfo("[DB] sqlite=%s", qUtf8Printable(path));
    if (!db.open()) {
        qWarning() << "preset db open failed" << db.lastError().text();
        return false;
    }
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS preset_position ("
        "position_number INTEGER PRIMARY KEY,"
        "name TEXT,"
        "longitude REAL,"
        "latitude REAL,"
        "address TEXT,"
        "create_time TEXT,"
        "update_time TEXT)"));
    return true;
}

void PresetStore::ensureSlots()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("usv"));
    QSqlQuery q(db);
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    db.transaction();
    for (int i = 1; i <= 255; ++i) {
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO preset_position(position_number,name,create_time,update_time) "
            "VALUES(?,?,?,?)"));
        q.addBindValue(i);
        q.addBindValue(QStringLiteral("预置位#%1").arg(i));
        q.addBindValue(now);
        q.addBindValue(now);
        q.exec();
    }
    db.commit();
}

void PresetStore::reload()
{
    beginResetModel();
    m_items.clear();
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.exec(QStringLiteral(
        "SELECT position_number,name,latitude,longitude,address FROM preset_position "
        "ORDER BY position_number"));
    while (q.next()) {
        PresetItem it;
        it.positionNumber = q.value(0).toInt();
        it.name = q.value(1).toString();
        it.latitude = q.value(2).toDouble();
        it.longitude = q.value(3).toDouble();
        it.address = q.value(4).toString();
        it.hasCoord = !q.value(2).isNull() && !q.value(3).isNull()
            && !(qFuzzyIsNull(it.latitude) && qFuzzyIsNull(it.longitude) && it.address.isEmpty());
        // 未写入过的槽位 latitude/longitude 可能是 NULL
        if (q.value(2).isNull() || q.value(3).isNull()) {
            it.hasCoord = false;
        }
        m_items.push_back(it);
    }
    endResetModel();
}

int PresetStore::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant PresetStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const PresetItem &it = m_items.at(index.row());
    switch (role) {
    case NumberRole:
        return it.positionNumber;
    case NameRole:
        return it.name;
    case LatitudeRole:
        return it.latitude;
    case LongitudeRole:
        return it.longitude;
    case AddressRole:
        return it.address;
    case HasCoordRole:
        return it.hasCoord;
    case LabelRole:
        return it.name.isEmpty() ? QStringLiteral("预置位#%1").arg(it.positionNumber) : it.name;
    case SubtitleRole:
        if (!it.hasCoord) {
            return QString();
        }
        return QStringLiteral("%1, %2").arg(it.latitude, 0, 'f', 6).arg(it.longitude, 0, 'f', 6);
    default:
        return {};
    }
}

QHash<int, QByteArray> PresetStore::roleNames() const
{
    return {
        {NumberRole, "positionNumber"},
        {NameRole, "name"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {AddressRole, "address"},
        {HasCoordRole, "hasCoord"},
        {LabelRole, "label"},
        {SubtitleRole, "subtitle"},
    };
}

bool PresetStore::saveCoord(int number, double lat, double lon, const QString &address)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral(
        "UPDATE preset_position SET latitude=?, longitude=?, address=?, update_time=? "
        "WHERE position_number=?"));
    q.addBindValue(lat);
    q.addBindValue(lon);
    q.addBindValue(address);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(number);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForNumber(number);
    if (row >= 0) {
        m_items[row].latitude = lat;
        m_items[row].longitude = lon;
        m_items[row].address = address;
        m_items[row].hasCoord = true;
        notifyRow(row);
    }
    return true;
}

bool PresetStore::clearCoord(int number)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral(
        "UPDATE preset_position SET latitude=NULL, longitude=NULL, address='', update_time=? "
        "WHERE position_number=?"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(number);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForNumber(number);
    if (row >= 0) {
        m_items[row].latitude = 0;
        m_items[row].longitude = 0;
        m_items[row].address.clear();
        m_items[row].hasCoord = false;
        notifyRow(row);
    }
    return true;
}

bool PresetStore::rename(int number, const QString &name)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("usv")));
    q.prepare(QStringLiteral(
        "UPDATE preset_position SET name=?, update_time=? WHERE position_number=?"));
    q.addBindValue(name);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(number);
    if (!q.exec()) {
        return false;
    }
    const int row = rowForNumber(number);
    if (row >= 0) {
        m_items[row].name = name;
        notifyRow(row);
    }
    return true;
}

int PresetStore::rowForNumber(int number) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).positionNumber == number) {
            return i;
        }
    }
    return -1;
}

void PresetStore::notifyRow(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

PresetItem PresetStore::itemAt(int number) const
{
    for (const auto &it : m_items) {
        if (it.positionNumber == number) {
            return it;
        }
    }
    return {};
}
