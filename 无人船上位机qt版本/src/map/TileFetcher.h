#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVector>
#include <functional>

class AppSettings;
class QNetworkAccessManager;
class QNetworkDiskCache;
class QNetworkReply;

class TileFetcher : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(const QImage &)>;

    explicit TileFetcher(AppSettings *settings, QObject *parent = nullptr);

    void request(const QString &providerId, const Done &done);

public slots:
    void cancelAll();
    void finishDecoded(const QString &key, const QImage &img);

private:
    struct Waiter {
        Done done;
    };
    struct Job {
        QUrl url;
        int prio = 9;
        quint64 seq = 0;
        QVector<Waiter> waiters;
    };

    struct Parsed {
        bool ok = false;
        int prio = 9;
        QString kind;
        QString layer;
        int z = 0;
        int x = 0;
        int y = 0;
        QUrl url;
    };

    void enqueue(const Parsed &parsed, const Done &done);
    void startNext();
    void onReplyFinished();
    void finishJob(const QString &key, const QImage &img);
    void remember(const QString &key, const QImage &img);
    QImage fromMemory(const QString &key);
    Parsed parseId(const QString &id) const;
    QUrl httpUrl(const QString &kind, const QString &layer, int z, int x, int y) const;

    AppSettings *m_settings = nullptr;
    QVector<QNetworkAccessManager *> m_nams;
    QNetworkDiskCache *m_disk = nullptr;
    QHash<QString, Job> m_queued;
    QHash<QString, Job> m_inflight;
    QHash<QString, QImage> m_mem;
    QStringList m_lru;
    QHash<QNetworkReply *, QString> m_replyKey;
    quint64 m_seq = 0;
    int m_namIndex = 0;

    static constexpr int kMaxInflight = 12;
    static constexpr int kMemTiles = 160;
    static constexpr int kNamCount = 4;
};
