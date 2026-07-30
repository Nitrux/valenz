#include "valenzbridge.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSslError>
#include <QTemporaryDir>

namespace
{
constexpr qint64 kArtworkDownloadLimit = 16 * 1024 * 1024;
constexpr int kArtworkTransferTimeoutMs = 3000;
constexpr int kArtworkTotalTimeoutMs = 5000;
constexpr int kArtworkRedirectLimit = 3;
constexpr int kArtworkDimensionLimit = 4096;
constexpr qint64 kArtworkPixelLimit = 16 * 1024 * 1024;
constexpr int kArtworkOutputDimensionLimit = 512;
constexpr int kArtworkCacheEntryLimit = 32;

bool imageDimensionsAreSafe(const QSize &size)
{
    return size.isValid()
        && size.width() > 0
        && size.height() > 0
        && size.width() <= kArtworkDimensionLimit
        && size.height() <= kArtworkDimensionLimit
        && static_cast<qint64>(size.width()) * size.height() <= kArtworkPixelLimit;
}

bool artworkFormatIsAllowed(const QByteArray &format)
{
    const QByteArray normalized = format.toLower();
    return normalized == "jpeg" || normalized == "jpg"
        || normalized == "png" || normalized == "webp"
        || normalized == "avif";
}

bool artworkMimeIsAllowed(const QByteArray &mimeType)
{
    const QByteArray normalized = mimeType.toLower();
    return normalized == "image/jpeg" || normalized == "image/jpg"
        || normalized == "image/png" || normalized == "image/webp"
        || normalized == "image/avif";
}

bool mimeMatchesArtworkFormat(const QByteArray &mimeType, const QByteArray &format)
{
    const QByteArray normalizedMime = mimeType.toLower();
    const QByteArray normalizedFormat = format.toLower();
    if (normalizedFormat == "jpeg" || normalizedFormat == "jpg")
        return normalizedMime == "image/jpeg" || normalizedMime == "image/jpg";
    if (normalizedFormat == "png")
        return normalizedMime == "image/png";
    if (normalizedFormat == "webp")
        return normalizedMime == "image/webp";
    if (normalizedFormat == "avif")
        return normalizedMime == "image/avif";

    return false;
}

bool httpsArtworkUrlIsAllowed(const QUrl &url)
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty()
        && url.userInfo().isEmpty()
        && url.port(443) == 443;
}

bool decodeNormalizedArtwork(QImageReader *reader,
                             QImage *normalizedImage,
                             QByteArray *detectedFormat = nullptr)
{
    if (!reader || !normalizedImage)
        return false;

    reader->setDecideFormatFromContent(true);
    if (!reader->canRead())
        return false;

    const QByteArray format = reader->format().toLower();
    if (!artworkFormatIsAllowed(format))
        return false;

    const QSize sourceSize = reader->size();
    if (!imageDimensionsAreSafe(sourceSize))
        return false;

    const QSize decodeSize = sourceSize.scaled(QSize(kArtworkOutputDimensionLimit,
                                                      kArtworkOutputDimensionLimit),
                                                Qt::KeepAspectRatio);
    reader->setScaledSize(decodeSize);

    QImage decoded = reader->read();
    if (decoded.isNull())
        return false;

    if (decoded.width() > kArtworkOutputDimensionLimit
        || decoded.height() > kArtworkOutputDimensionLimit)
    {
        decoded = decoded.scaled(QSize(kArtworkOutputDimensionLimit,
                                       kArtworkOutputDimensionLimit),
                                 Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    }

    if (decoded.isNull() || decoded.width() <= 0 || decoded.height() <= 0
        || decoded.width() > kArtworkOutputDimensionLimit
        || decoded.height() > kArtworkOutputDimensionLimit)
    {
        return false;
    }

    *normalizedImage = decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (normalizedImage->isNull())
        return false;

    if (detectedFormat)
        *detectedFormat = format;
    return true;
}

bool validateLocalArtwork(const QString &path)
{
    const QFileInfo fileInfo(path);
    if (!fileInfo.isFile() || !fileInfo.isReadable()
        || fileInfo.size() <= 0 || fileInfo.size() > kArtworkDownloadLimit)
    {
        return false;
    }

    QImageReader reader(fileInfo.absoluteFilePath());
    QImage normalizedImage;
    return decodeNormalizedArtwork(&reader, &normalizedImage);
}
}

void ValenzBridge::requestMediaArtwork(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty())
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = {};
        updateMediaArtSource(QString());
        return;
    }

    QUrl url(trimmed);
    if (url.scheme().isEmpty() && QFileInfo(trimmed).isAbsolute())
        url = QUrl::fromLocalFile(trimmed);

    if (url.isLocalFile()
        && (url.host().isEmpty() || url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0))
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = {};
        const QString localPath = url.toLocalFile();
        updateMediaArtSource(validateLocalArtwork(localPath)
                                 ? QUrl::fromLocalFile(QFileInfo(localPath).absoluteFilePath()).toString()
                                 : QString());
        return;
    }

    if (url.scheme().compare(QStringLiteral("qrc"), Qt::CaseInsensitive) == 0)
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = {};
        updateMediaArtSource(url.toString());
        return;
    }

    if (!httpsArtworkUrlIsAllowed(url))
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = {};
        updateMediaArtSource(QString());
        return;
    }

    const QString cacheKey = url.toString(QUrl::FullyEncoded);
    const QString cachedSource = m_artworkCache.value(cacheKey);
    if (!cachedSource.isEmpty() && QFileInfo(QUrl(cachedSource).toLocalFile()).isFile())
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = {};
        m_artworkCacheOrder.removeAll(cacheKey);
        m_artworkCacheOrder.append(cacheKey);
        updateMediaArtSource(cachedSource);
        return;
    }

    if (m_artworkReply && m_artworkRequestedUrl == url)
        return;

    cancelMediaArtworkRequest();
    m_artworkRequestedUrl = url;
    m_artworkRedirectCount = 0;
    updateMediaArtSource(QString());
    if (m_artworkTimeoutTimer)
        m_artworkTimeoutTimer->start(kArtworkTotalTimeoutMs);
    startMediaArtworkRequest(url);
}

void ValenzBridge::startMediaArtworkRequest(const QUrl &url)
{
    if (!m_artworkNetwork || !httpsArtworkUrlIsAllowed(url))
        return;

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(kArtworkTransferTimeoutMs);
    request.setRawHeader("Accept", "image/avif,image/webp,image/png,image/jpeg");

    QNetworkReply *reply = m_artworkNetwork->get(request);
    m_artworkReply = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        handleMediaArtworkReadyRead(reply);
    });
    connect(reply, &QNetworkReply::sslErrors, this,
            [reply](const QList<QSslError> &) {
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMediaArtworkFinished(reply);
    });
}

void ValenzBridge::handleMediaArtworkReadyRead(QNetworkReply *reply)
{
    if (!reply || reply != m_artworkReply)
        return;

    const qint64 remaining = kArtworkDownloadLimit - m_artworkDownloadBuffer.size();
    m_artworkDownloadBuffer += reply->read(remaining + 1);
    if (m_artworkDownloadBuffer.size() > kArtworkDownloadLimit)
        reply->abort();
}

void ValenzBridge::handleMediaArtworkFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    if (reply != m_artworkReply)
    {
        reply->deleteLater();
        return;
    }

    handleMediaArtworkReadyRead(reply);
    m_artworkReply = nullptr;

    const QVariant redirectAttribute = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirectAttribute.isValid() && reply->error() == QNetworkReply::NoError)
    {
        const QUrl redirectUrl = reply->url().resolved(redirectAttribute.toUrl());
        reply->deleteLater();

        if (++m_artworkRedirectCount <= kArtworkRedirectLimit
            && httpsArtworkUrlIsAllowed(redirectUrl))
        {
            m_artworkDownloadBuffer.clear();
            startMediaArtworkRequest(redirectUrl);
            return;
        }

        if (m_artworkTimeoutTimer)
            m_artworkTimeoutTimer->stop();
        m_artworkDownloadBuffer.clear();
        m_artworkRequestedUrl = {};
        return;
    }

    if (m_artworkTimeoutTimer)
        m_artworkTimeoutTimer->stop();

    const QVariant declaredLengthHeader = reply->header(QNetworkRequest::ContentLengthHeader);
    const qint64 declaredLength = declaredLengthHeader.isValid() ? declaredLengthHeader.toLongLong() : -1;
    const QByteArray mimeType = reply->header(QNetworkRequest::ContentTypeHeader)
                                    .toByteArray().section(';', 0, 0).trimmed().toLower();
    const bool responseIsUsable = reply->error() == QNetworkReply::NoError
        && (declaredLength < 0 || declaredLength <= kArtworkDownloadLimit)
        && !m_artworkDownloadBuffer.isEmpty()
        && m_artworkDownloadBuffer.size() <= kArtworkDownloadLimit
        && artworkMimeIsAllowed(mimeType);
    reply->deleteLater();

    if (!responseIsUsable || !m_artworkCacheDir || !m_artworkCacheDir->isValid())
    {
        m_artworkDownloadBuffer.clear();
        m_artworkRequestedUrl = {};
        return;
    }

    QBuffer imageBuffer(&m_artworkDownloadBuffer);
    imageBuffer.open(QIODevice::ReadOnly);
    QImageReader reader(&imageBuffer);
    QImage normalizedImage;
    QByteArray detectedFormat;
    if (!decodeNormalizedArtwork(&reader, &normalizedImage, &detectedFormat)
        || !mimeMatchesArtworkFormat(mimeType, detectedFormat))
    {
        m_artworkDownloadBuffer.clear();
        m_artworkRequestedUrl = {};
        return;
    }

    const QString cacheKey = m_artworkRequestedUrl.toString(QUrl::FullyEncoded);
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(cacheKey.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString cachePath = m_artworkCacheDir->filePath(digest + QStringLiteral(".png"));
    QSaveFile cacheFile(cachePath);
    if (!cacheFile.open(QIODevice::WriteOnly)
        || !normalizedImage.save(&cacheFile, "PNG")
        || !cacheFile.commit())
    {
        m_artworkDownloadBuffer.clear();
        m_artworkRequestedUrl = {};
        return;
    }

    while (m_artworkCacheOrder.size() >= kArtworkCacheEntryLimit)
    {
        const QString oldestKey = m_artworkCacheOrder.takeFirst();
        const QString oldestSource = m_artworkCache.take(oldestKey);
        QFile::remove(QUrl(oldestSource).toLocalFile());
    }

    const QString localSource = QUrl::fromLocalFile(cachePath).toString();
    m_artworkCache.insert(cacheKey, localSource);
    m_artworkCacheOrder.append(cacheKey);
    m_artworkDownloadBuffer.clear();
    m_artworkRequestedUrl = {};
    updateMediaArtSource(localSource);
}

void ValenzBridge::cancelMediaArtworkRequest()
{
    if (m_artworkTimeoutTimer)
        m_artworkTimeoutTimer->stop();

    if (m_artworkReply)
    {
        disconnect(m_artworkReply, nullptr, this, nullptr);
        m_artworkReply->abort();
        m_artworkReply->deleteLater();
        m_artworkReply = nullptr;
    }

    m_artworkDownloadBuffer.clear();
}
