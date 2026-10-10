#include "map/TileImageProvider.h"

#include "map/TileFetcher.h"

#include <QPointer>

QQuickTextureFactory *TileResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void TileResponse::cancel()
{
    m_cancelled = true;
}

void TileResponse::complete(const QImage &img)
{
    if (!m_cancelled) {
        m_image = img;
    }
    emit finished();
}

TileImageProvider::TileImageProvider(TileFetcher *fetcher)
    : m_fetcher(fetcher)
{
}

QQuickImageResponse *TileImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    auto *resp = new TileResponse;
    if (!m_fetcher) {
        QMetaObject::invokeMethod(resp, "complete", Qt::QueuedConnection, Q_ARG(QImage, QImage()));
        return resp;
    }
    QPointer<TileResponse> guard(resp);
    m_fetcher->request(id, [guard](const QImage &img) {
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), "complete", Qt::QueuedConnection, Q_ARG(QImage, img));
    });
    return resp;
}
