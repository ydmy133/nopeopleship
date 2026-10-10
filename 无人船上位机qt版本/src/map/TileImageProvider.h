#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQuickAsyncImageProvider>
#else
#include <QQuickImageProvider>
#endif

class TileFetcher;

class TileResponse : public QQuickImageResponse
{
    Q_OBJECT

public:
    QQuickTextureFactory *textureFactory() const override;
    void cancel() override;

public slots:
    void complete(const QImage &img);

private:
    QImage m_image;
    bool m_cancelled = false;
};

class TileImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit TileImageProvider(TileFetcher *fetcher);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    TileFetcher *m_fetcher = nullptr;
};
