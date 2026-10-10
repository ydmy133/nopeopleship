#include "map/TileFetcher.h"

#include "settings/AppSettings.h"

#include <QDir>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRunnable>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QtGlobal>
#include <algorithm>

namespace {

class DecodeTask : public QRunnable
{
public:
    DecodeTask(QByteArray bytes, QString key, TileFetcher *fetcher)
        : m_bytes(std::move(bytes))
        , m_key(std::move(key))
        , m_fetcher(fetcher)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QImage img;
        img.loadFromData(m_bytes);
        if (!img.isNull() && img.format() != QImage::Format_ARGB32_Premultiplied) {
            img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        QMetaObject::invokeMethod(m_fetcher, "finishDecoded", Qt::QueuedConnection,
                                  Q_ARG(QString, m_key), Q_ARG(QImage, img));
    }

private:
    QByteArray m_bytes;
    QString m_key;
    TileFetcher *m_fetcher = nullptr;
};

} // namespace

TileFetcher::TileFetcher(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_disk = new QNetworkDiskCache(this);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/map-tiles");
    QDir().mkpath(dir);
    m_disk->setCacheDirectory(dir);
    m_disk->setMaximumCacheSize(512LL * 1024 * 1024);

    m_nams.reserve(kNamCount);
    for (int i = 0; i < kNamCount; ++i) {
        auto *nam = new QNetworkAccessManager(this);
        nam->setCache(m_disk);
        m_nams.push_back(nam);
    }
}

void TileFetcher::request(const QString &providerId, const Done &done)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, providerId, done]() {
                request(providerId, done);
            },
            Qt::QueuedConnection);
        return;
    }
    const Parsed parsed = parseId(providerId);
    if (!parsed.ok) {
        if (done) {
            done(QImage());
        }
        return;
    }
    enqueue(parsed, done);
}

void TileFetcher::enqueue(const Parsed &parsed, const Done &done)
{
    const QString key = parsed.url.toString();
    const QImage cached = fromMemory(key);
    if (!cached.isNull()) {
        if (done) {
            done(cached);
        }
        return;
    }

    auto queuedIt = m_queued.find(key);
    if (queuedIt != m_queued.end()) {
        queuedIt->waiters.push_back(Waiter{done});
        queuedIt->prio = std::min(queuedIt->prio, parsed.prio);
        return;
    }
    auto flyIt = m_inflight.find(key);
    if (flyIt != m_inflight.end()) {
        flyIt->waiters.push_back(Waiter{done});
        return;
    }

    Job job;
    job.url = parsed.url;
    job.prio = parsed.prio;
    job.seq = ++m_seq;
    job.waiters.push_back(Waiter{done});
    m_queued.insert(key, job);
    startNext();
}

void TileFetcher::cancelAll()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "cancelAll", Qt::QueuedConnection);
        return;
    }

    QHash<QString, Job> dropped = m_queued;
    m_queued.clear();
    for (auto it = m_inflight.constBegin(); it != m_inflight.constEnd(); ++it) {
        dropped.insert(it.key(), it.value());
    }
    m_inflight.clear();

    const QList<QNetworkReply *> replies = m_replyKey.keys();
    m_replyKey.clear();
    for (QNetworkReply *reply : replies) {
        if (!reply) {
            continue;
        }
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    for (auto it = dropped.constBegin(); it != dropped.constEnd(); ++it) {
        for (const Waiter &w : it->waiters) {
            if (w.done) {
                w.done(QImage());
            }
        }
    }
}

void TileFetcher::startNext()
{
    while (m_inflight.size() < kMaxInflight && !m_queued.isEmpty()) {
        QString bestKey;
        int bestPrio = 1000;
        quint64 bestSeq = ~quint64(0);
        for (auto it = m_queued.constBegin(); it != m_queued.constEnd(); ++it) {
            if (it->prio < bestPrio || (it->prio == bestPrio && it->seq < bestSeq)) {
                bestPrio = it->prio;
                bestSeq = it->seq;
                bestKey = it.key();
            }
        }
        if (bestKey.isEmpty()) {
            break;
        }
        Job job = m_queued.take(bestKey);
        QNetworkAccessManager *nam = m_nams.at(m_namIndex % m_nams.size());
        m_namIndex++;

        QNetworkRequest req(job.url);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("usv_gcs/1.0"));
        req.setRawHeader("Accept", "image/png,image/jpeg,image/*;q=0.8,*/*;q=0.1");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        req.setTransferTimeout(8000);
#endif

        QNetworkReply *reply = nam->get(req);
        m_replyKey.insert(reply, bestKey);
        m_inflight.insert(bestKey, job);
        connect(reply, &QNetworkReply::finished, this, &TileFetcher::onReplyFinished);
    }
}

void TileFetcher::onReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    const QString key = m_replyKey.take(reply);
    if (key.isEmpty()) {
        startNext();
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        finishJob(key, QImage());
        startNext();
        return;
    }
    const QByteArray bytes = reply->readAll();
    if (bytes.size() < 24 || bytes.startsWith('<')) {
        finishJob(key, QImage());
        startNext();
        return;
    }
    auto *task = new DecodeTask(bytes, key, this);
    QThreadPool::globalInstance()->start(task);
}

void TileFetcher::finishDecoded(const QString &key, const QImage &img)
{
    finishJob(key, img);
    startNext();
}

void TileFetcher::finishJob(const QString &key, const QImage &img)
{
    if (!img.isNull()) {
        remember(key, img);
    }
    const Job job = m_inflight.take(key);
    for (const Waiter &w : job.waiters) {
        if (w.done) {
            w.done(img);
        }
    }
}

void TileFetcher::remember(const QString &key, const QImage &img)
{
    if (m_mem.contains(key)) {
        m_lru.removeOne(key);
    }
    m_mem.insert(key, img);
    m_lru.append(key);
    while (m_mem.size() > kMemTiles && !m_lru.isEmpty()) {
        const QString old = m_lru.takeFirst();
        m_mem.remove(old);
    }
}

QImage TileFetcher::fromMemory(const QString &key)
{
    const auto it = m_mem.constFind(key);
    if (it == m_mem.cend()) {
        return {};
    }
    m_lru.removeOne(key);
    m_lru.append(key);
    return it.value();
}

TileFetcher::Parsed TileFetcher::parseId(const QString &id) const
{
    Parsed p;
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 6) {
        return p;
    }
    bool okPrio = false;
    bool okZ = false;
    bool okX = false;
    bool okY = false;
    p.prio = parts.at(0).toInt(&okPrio);
    p.kind = parts.at(1);
    p.layer = parts.at(2);
    p.z = parts.at(3).toInt(&okZ);
    p.x = parts.at(4).toInt(&okX);
    p.y = parts.at(5).toInt(&okY);
    if (!okPrio || !okZ || !okX || !okY || p.kind.isEmpty() || p.layer.isEmpty()) {
        return p;
    }
    p.url = httpUrl(p.kind, p.layer, p.z, p.x, p.y);
    p.ok = p.url.isValid();
    return p;
}

QUrl TileFetcher::httpUrl(const QString &kind, const QString &layer, int z, int x, int y) const
{
    QString host = m_settings ? m_settings->mapBaseUrl() : QStringLiteral("http://210.76.81.182");
    host = host.trimmed();
    while (host.endsWith(QLatin1Char('/'))) {
        host.chop(1);
    }
    if (host.isEmpty()) {
        host = QStringLiteral("http://210.76.81.182");
    }
    const QString url = QStringLiteral("%1/kinth-map/%2/%3/%4/%5/%6.png")
                            .arg(host, kind, layer)
                            .arg(z)
                            .arg(x)
                            .arg(y);
    return QUrl(url);
}
