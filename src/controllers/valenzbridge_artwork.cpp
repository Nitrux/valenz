// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "valenzbridge.h"

#include <QBuffer>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
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
#include <QTimer>

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
constexpr auto kBluezObexService = "org.bluez.obex";
constexpr auto kBluezObexRootPath = "/org/bluez/obex";
constexpr auto kBluezObexClientInterface = "org.bluez.obex.Client1";
constexpr auto kBluezObexImageInterface = "org.bluez.obex.Image1";
constexpr auto kBluezObexTransferInterface = "org.bluez.obex.Transfer1";
constexpr auto kDbusPropertiesInterface = "org.freedesktop.DBus.Properties";

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

void ValenzBridge::requestBluezMediaArtwork(const QString &playerPath,
                                             const QString &deviceAddress,
                                             const QString &handle,
                                             quint16 obexPort)
{
    if (playerPath.isEmpty())
        return;

    if (deviceAddress.isEmpty() || handle.isEmpty() || obexPort == 0
        || !m_artworkCacheDir || !m_artworkCacheDir->isValid())
    {
        if (m_bluezArtworkPlayerPath == playerPath)
        {
            ++m_bluezArtworkRequestId;
            cancelBluezArtworkTransfer();
            resetBluezArtworkSession();
            m_bluezArtworkPlayerPath.clear();
            m_bluezArtworkDeviceAddress.clear();
            m_bluezArtworkHandle.clear();
            m_bluezArtworkTargetPath.clear();
            m_bluezArtworkSource.clear();
            m_bluezArtworkObexPort = 0;
            m_bluezArtworkRequestPending = false;
            updateMediaArtSource(QString());
        }
        return;
    }

    const bool sameRequest = m_bluezArtworkPlayerPath == playerPath
        && m_bluezArtworkDeviceAddress == deviceAddress
        && m_bluezArtworkHandle == handle
        && m_bluezArtworkObexPort == obexPort;
    if (sameRequest && (m_bluezArtworkRequestPending || !m_bluezArtworkSource.isEmpty()))
        return;

    const bool sameSession = sameRequest && !m_bluezArtworkSessionPath.isEmpty();
    cancelBluezArtworkTransfer();
    ++m_bluezArtworkRequestId;
    m_bluezArtworkPlayerPath = playerPath;
    m_bluezArtworkDeviceAddress = deviceAddress;
    m_bluezArtworkHandle = handle;
    m_bluezArtworkObexPort = obexPort;
    m_bluezArtworkSource.clear();
    m_bluezArtworkTargetPath = m_artworkCacheDir->filePath(
        QString::fromLatin1(QCryptographicHash::hash((playerPath + QLatin1Char(':') + handle).toUtf8(),
                                                      QCryptographicHash::Sha256).toHex())
        + QStringLiteral(".img"));
    QFile::remove(m_bluezArtworkTargetPath);
    cancelMediaArtworkRequest();
    updateMediaArtSource(QString());

    if (sameSession)
    {
        startBluezArtworkTransfer(m_bluezArtworkRequestId);
        return;
    }

    resetBluezArtworkSession();
    const quint64 requestId = m_bluezArtworkRequestId;
    m_bluezArtworkRequestPending = true;
    QVariantMap options;
    options.insert(QStringLiteral("Target"), QStringLiteral("bip-avrcp"));
    options.insert(QStringLiteral("PSM"), QVariant::fromValue(obexPort));

    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kBluezObexService),
                                                       QString::fromLatin1(kBluezObexRootPath),
                                                       QString::fromLatin1(kBluezObexClientInterface),
                                                       QStringLiteral("CreateSession"));
    call << deviceAddress << options;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (requestId != m_bluezArtworkRequestId)
            return;
        if (reply.isError())
        {
            m_bluezArtworkRequestPending = false;
            return;
        }

        m_bluezArtworkSessionPath = reply.value().path();
        startBluezArtworkTransfer(requestId);
    });
}

void ValenzBridge::startBluezArtworkTransfer(quint64 requestId)
{
    if (requestId != m_bluezArtworkRequestId || m_bluezArtworkSessionPath.isEmpty()
        || m_bluezArtworkTargetPath.isEmpty())
    {
        return;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kBluezObexService),
                                                       m_bluezArtworkSessionPath,
                                                       QString::fromLatin1(kBluezObexImageInterface),
                                                       QStringLiteral("Get"));
    call << m_bluezArtworkTargetPath << m_bluezArtworkHandle << QVariantMap();
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<QDBusObjectPath, QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (requestId != m_bluezArtworkRequestId)
            return;
        if (reply.isError())
        {
            m_bluezArtworkRequestPending = false;
            resetBluezArtworkSession();
            return;
        }

        m_bluezArtworkTransferPath = reply.argumentAt<0>().path();
        const bool connected = QDBusConnection::sessionBus().connect(
            QString::fromLatin1(kBluezObexService),
            m_bluezArtworkTransferPath,
            QString::fromLatin1(kDbusPropertiesInterface),
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(onBluezArtworkTransferPropertiesChanged(QString,QVariantMap,QStringList)));
        if (!connected)
        {
            finishBluezArtworkTransfer(false);
            return;
        }

        QDBusInterface propertiesIface(QString::fromLatin1(kBluezObexService),
                                       m_bluezArtworkTransferPath,
                                       QString::fromLatin1(kDbusPropertiesInterface),
                                       QDBusConnection::sessionBus());
        const QDBusReply<QDBusVariant> statusReply = propertiesIface.call(
            QStringLiteral("Get"),
            QString::fromLatin1(kBluezObexTransferInterface),
            QStringLiteral("Status"));
        if (!statusReply.isValid())
            return;

        const QString status = statusReply.value().variant().toString();
        if (status.compare(QStringLiteral("complete"), Qt::CaseInsensitive) == 0)
            finishBluezArtworkTransfer(true);
        else if (status.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0)
            finishBluezArtworkTransfer(false);
        else
            m_bluezArtworkRequestPending = true;
    });
}

void ValenzBridge::onBluezArtworkTransferPropertiesChanged(const QString &interfaceName,
                                                           const QVariantMap &changedProperties,
                                                           const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)
    if (interfaceName != QString::fromLatin1(kBluezObexTransferInterface))
        return;

    const QString status = changedProperties.value(QStringLiteral("Status")).toString();
    if (status.compare(QStringLiteral("complete"), Qt::CaseInsensitive) == 0)
        finishBluezArtworkTransfer(true);
    else if (status.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0)
        finishBluezArtworkTransfer(false);
}

void ValenzBridge::finishBluezArtworkTransfer(bool success)
{
    if (!m_bluezArtworkTransferPath.isEmpty())
    {
        QDBusConnection::sessionBus().disconnect(
            QString::fromLatin1(kBluezObexService),
            m_bluezArtworkTransferPath,
            QString::fromLatin1(kDbusPropertiesInterface),
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(onBluezArtworkTransferPropertiesChanged(QString,QVariantMap,QStringList)));
    }
    m_bluezArtworkTransferPath.clear();
    m_bluezArtworkRequestPending = false;

    if (!success || !validateLocalArtwork(m_bluezArtworkTargetPath))
    {
        QFile::remove(m_bluezArtworkTargetPath);
        resetBluezArtworkSession();
        return;
    }

    m_bluezArtworkSource = QUrl::fromLocalFile(m_bluezArtworkTargetPath).toString();
    updateMediaArtSource(m_bluezArtworkSource);
}

void ValenzBridge::cancelBluezArtworkTransfer()
{
    if (m_bluezArtworkTransferPath.isEmpty())
        return;

    QDBusConnection::sessionBus().disconnect(
        QString::fromLatin1(kBluezObexService),
        m_bluezArtworkTransferPath,
        QString::fromLatin1(kDbusPropertiesInterface),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onBluezArtworkTransferPropertiesChanged(QString,QVariantMap,QStringList)));

    QDBusMessage cancel = QDBusMessage::createMethodCall(QString::fromLatin1(kBluezObexService),
                                                         m_bluezArtworkTransferPath,
                                                         QString::fromLatin1(kBluezObexTransferInterface),
                                                         QStringLiteral("Cancel"));
    QDBusConnection::sessionBus().asyncCall(cancel);
    m_bluezArtworkTransferPath.clear();
    m_bluezArtworkRequestPending = false;
}

void ValenzBridge::resetBluezArtworkSession()
{
    cancelBluezArtworkTransfer();
    if (!m_bluezArtworkSessionPath.isEmpty())
    {
        QDBusMessage remove = QDBusMessage::createMethodCall(QString::fromLatin1(kBluezObexService),
                                                             QString::fromLatin1(kBluezObexRootPath),
                                                             QString::fromLatin1(kBluezObexClientInterface),
                                                             QStringLiteral("RemoveSession"));
        remove << QVariant::fromValue(QDBusObjectPath(m_bluezArtworkSessionPath));
        QDBusConnection::sessionBus().asyncCall(remove);
    }
    m_bluezArtworkSessionPath.clear();
}

void ValenzBridge::requestMediaArtwork(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty())
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = QUrl();
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
        m_artworkRequestedUrl = QUrl();
        const QString localPath = url.toLocalFile();
        updateMediaArtSource(validateLocalArtwork(localPath)
                                 ? QUrl::fromLocalFile(QFileInfo(localPath).absoluteFilePath()).toString()
                                 : QString());
        return;
    }

    if (url.scheme().compare(QStringLiteral("qrc"), Qt::CaseInsensitive) == 0)
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = QUrl();
        updateMediaArtSource(url.toString());
        return;
    }

    if (!httpsArtworkUrlIsAllowed(url))
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = QUrl();
        updateMediaArtSource(QString());
        return;
    }

    const QString cacheKey = url.toString(QUrl::FullyEncoded);
    const QString cachedSource = m_artworkCache.value(cacheKey);
    if (!cachedSource.isEmpty() && QFileInfo(QUrl(cachedSource).toLocalFile()).isFile())
    {
        cancelMediaArtworkRequest();
        m_artworkRequestedUrl = QUrl();
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
        m_artworkRequestedUrl = QUrl();
        return;
    }

    if (m_artworkTimeoutTimer)
        m_artworkTimeoutTimer->stop();

    const QVariant declaredLengthHeader = reply->header(QNetworkRequest::ContentLengthHeader);
    const qint64 declaredLength = declaredLengthHeader.isValid() ? declaredLengthHeader.toLongLong() : -1;
    QByteArray mimeType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
    const qsizetype mimeParameterIndex = mimeType.indexOf(';');
    if (mimeParameterIndex >= 0) {
        mimeType.truncate(mimeParameterIndex);
    }
    mimeType = mimeType.trimmed().toLower();
    const bool responseIsUsable = reply->error() == QNetworkReply::NoError
        && (declaredLength < 0 || declaredLength <= kArtworkDownloadLimit)
        && !m_artworkDownloadBuffer.isEmpty()
        && m_artworkDownloadBuffer.size() <= kArtworkDownloadLimit
        && artworkMimeIsAllowed(mimeType);
    reply->deleteLater();

    if (!responseIsUsable || !m_artworkCacheDir || !m_artworkCacheDir->isValid())
    {
        m_artworkDownloadBuffer.clear();
        m_artworkRequestedUrl = QUrl();
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
        m_artworkRequestedUrl = QUrl();
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
        m_artworkRequestedUrl = QUrl();
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
    m_artworkRequestedUrl = QUrl();
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
