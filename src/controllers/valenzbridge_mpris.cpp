#include "valenzbridge.h"
#include "valenzbridge_p.h"

void ValenzBridge::connectMprisSignalObservers()
{
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kDbusService),
                                          QString::fromLatin1(kDbusPath),
                                          QString::fromLatin1(kDbusInterface),
                                          QStringLiteral("NameOwnerChanged"),
                                          this,
                                          SLOT(onMprisNameOwnerChanged(QString,QString,QString)));
}

void ValenzBridge::updateMprisPropertiesSubscription(const QString &serviceName)
{
    if (m_mprisPropertiesServiceName == serviceName)
        return;

    clearMprisPropertiesSubscription();

    if (serviceName.isEmpty())
        return;

    const bool connected = QDBusConnection::sessionBus().connect(serviceName,
                                                                 QString::fromLatin1(kMprisObjectPath),
                                                                 QString::fromLatin1(kDbusPropertiesInterface),
                                                                 QStringLiteral("PropertiesChanged"),
                                                                 this,
                                                                 SLOT(onMprisPropertiesChanged(QString,QVariantMap,QStringList)));
    if (!connected)
    {
        return;
    }

    m_mprisPropertiesServiceName = serviceName;
}

void ValenzBridge::clearMprisPropertiesSubscription()
{
    if (m_mprisPropertiesServiceName.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(m_mprisPropertiesServiceName,
                                             QString::fromLatin1(kMprisObjectPath),
                                             QString::fromLatin1(kDbusPropertiesInterface),
                                             QStringLiteral("PropertiesChanged"),
                                             this,
                                             SLOT(onMprisPropertiesChanged(QString,QVariantMap,QStringList)));

    m_mprisPropertiesServiceName.clear();
}

void ValenzBridge::clearMprisState()
{
    m_mprisServiceName.clear();
    m_mprisTrackLengthUs = 0;
    m_mprisPositionUs = 0;
    m_mprisLastPositionUs = 0;
    m_mprisLastPositionEpochMs = 0;
    setMediaTitle(QString());
    setMediaArtist(QString());
    setMediaArtSource(QString());
    setMediaTimestamp(QString());
    setMediaPlaying(false);
    setMprisVisible(false);
    setMprisSources({});
    updateMprisPlaybackTicker();
}

void ValenzBridge::setMprisSources(const QVariantList &sources)
{
    if (m_mprisSources == sources)
        return;

    m_mprisSources = sources;
    Q_EMIT mprisSourcesChanged();
}

void ValenzBridge::updateMprisPlaybackTicker()
{
    if (!m_mprisPlaybackTimer)
        return;

    const bool shouldRun = m_mprisVisible && m_mediaPlaying && !m_mprisServiceName.isEmpty();

    if (shouldRun && !m_mprisPlaybackTimer->isActive())
        m_mprisPlaybackTimer->start();
    else if (!shouldRun && m_mprisPlaybackTimer->isActive())
        m_mprisPlaybackTimer->stop();
}

void ValenzBridge::updateMprisTimestampFromTicker()
{
    if (!m_mprisVisible || !m_mediaPlaying || m_mprisServiceName.isEmpty())
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_mprisLastPositionEpochMs <= 0 || nowMs <= m_mprisLastPositionEpochMs)
        return;

    const qint64 elapsedUs = (nowMs - m_mprisLastPositionEpochMs) * 1000;
    qint64 projectedUs = qMax<qint64>(0, m_mprisLastPositionUs + elapsedUs);

    if (m_mprisTrackLengthUs > 0)
        projectedUs = qMin(projectedUs, m_mprisTrackLengthUs);

    if (projectedUs == m_mprisPositionUs)
        return;

    const QString previousTimestamp = m_mediaTimestamp;
    m_mprisPositionUs = projectedUs;

    const QString timestamp = formatMprisTimestamp(m_mprisPositionUs, m_mprisTrackLengthUs);
    if (timestamp != previousTimestamp)
        setMediaTimestamp(timestamp);
}


void ValenzBridge::onMprisPropertiesChanged(const QString &interfaceName,
                                            const QVariantMap &changedProperties,
                                            const QStringList &invalidatedProperties)
{
    if (interfaceName != QString::fromLatin1(kMprisPlayerInterface))
        return;
    refreshMprisState();
}

void ValenzBridge::onMprisNameOwnerChanged(const QString &serviceName,
                                           const QString &oldOwner,
                                           const QString &newOwner)
{
    if (!serviceName.startsWith(QString::fromLatin1(kMprisServicePrefix)))
        return;
    refreshMprisState();
}

QStringList ValenzBridge::mprisServiceNames() const
{
    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    if (!busInterface)
    {
        return {};
    }

    const QDBusReply<QStringList> namesReply = busInterface->registeredServiceNames();
    if (!namesReply.isValid())
    {
        return {};
    }

    QStringList services;
    const QStringList allServices = namesReply.value();

    for (const QString &service : allServices)
    {
        if (service.startsWith(QString::fromLatin1(kMprisServicePrefix)))
            services << service;
    }
    return services;
}

QVariantMap ValenzBridge::mprisPlayerProperties(const QString &serviceName) const
{
    if (serviceName.isEmpty())
        return {};

    QDBusInterface propertiesIface(serviceName,
                                   QString::fromLatin1(kMprisObjectPath),
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::sessionBus());

    const QDBusReply<QVariantMap> reply = propertiesIface.call(QStringLiteral("GetAll"),
                                                                QString::fromLatin1(kMprisPlayerInterface));

    if (!reply.isValid())
    {
        return {};
    }

    return reply.value();
}

QVariantMap ValenzBridge::mprisPlayerMetadata(const QString &serviceName) const
{
    if (serviceName.isEmpty())
        return {};

    QDBusInterface propertiesIface(serviceName,
                                   QString::fromLatin1(kMprisObjectPath),
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::sessionBus());

    const QDBusReply<QDBusVariant> reply = propertiesIface.call(QStringLiteral("Get"),
                                                                QString::fromLatin1(kMprisPlayerInterface),
                                                                QStringLiteral("Metadata"));

    if (!reply.isValid())
    {
        return {};
    }

    return variantToVariantMap(reply.value().variant());
}

QString ValenzBridge::preferredMprisService() const
{
    const QStringList services = mprisServiceNames();
    if (services.isEmpty())
        return {};

    QString fallbackService = services.constFirst();

    for (const QString &service : services)
    {
        const QVariantMap properties = mprisPlayerProperties(service);
        const QString playbackStatus = unwrapMprisVariant(properties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed();
        if (playbackStatus.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0)
        {
            return service;
        }

        if (fallbackService.isEmpty() && !properties.isEmpty())
            fallbackService = service;
    }
    return fallbackService;
}

qint64 ValenzBridge::mprisPlayerPositionUs(const QString &serviceName) const
{
    if (serviceName.isEmpty())
        return 0;

    QDBusInterface propertiesIface(serviceName,
                                   QString::fromLatin1(kMprisObjectPath),
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::sessionBus());

    const QDBusReply<QDBusVariant> reply = propertiesIface.call(QStringLiteral("Get"),
                                                                QString::fromLatin1(kMprisPlayerInterface),
                                                                QStringLiteral("Position"));

    if (!reply.isValid())
        return 0;

    return qMax<qint64>(0, reply.value().variant().toLongLong());
}

QString ValenzBridge::formatMprisTimeUs(qint64 microseconds) const
{
    const qint64 totalSeconds = qMax<qint64>(0, microseconds / 1000000);
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;

    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

QString ValenzBridge::formatMprisTimestamp(qint64 positionUs, qint64 lengthUs) const
{
    const QString left = formatMprisTimeUs(positionUs);
    const QString right = lengthUs > 0 ? formatMprisTimeUs(lengthUs) : QStringLiteral("--:--");
    return left + QStringLiteral(" / ") + right;
}

bool ValenzBridge::invokeMprisPlayerMethod(const QString &method)
{
    if (m_mprisServiceName.isEmpty())
        refreshMprisState();

    if (m_mprisServiceName.isEmpty())
        return false;

    QDBusInterface playerIface(m_mprisServiceName,
                               QString::fromLatin1(kMprisObjectPath),
                               QString::fromLatin1(kMprisPlayerInterface),
                               QDBusConnection::sessionBus());
    const QDBusMessage result = playerIface.call(method);
    if (result.type() == QDBusMessage::ErrorMessage)
    {
        return false;
    }

    return true;
}

void ValenzBridge::refreshMprisState()
{
    const QStringList services = mprisServiceNames();
    if (services.isEmpty())
    {
        clearMprisPropertiesSubscription();
        clearMprisState();
        return;
    }

    QString serviceName = m_mprisServiceName;
    if (serviceName.isEmpty() || !services.contains(serviceName))
        serviceName = preferredMprisService();

    if (serviceName.isEmpty())
    {
        clearMprisPropertiesSubscription();
        clearMprisState();
        return;
    }

    updateMprisPropertiesSubscription(serviceName);

    const QVariantMap playerProperties = mprisPlayerProperties(serviceName);
    if (playerProperties.isEmpty())
    {
        clearMprisState();
        return;
    }

    m_mprisServiceName = serviceName;

    const QString playbackStatus = unwrapMprisVariant(playerProperties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed();
    const bool isPlaying = playbackStatus.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0;

    QVariantMap metadata = mprisPlayerMetadata(serviceName);
    if (metadata.isEmpty())
        metadata = variantToVariantMap(playerProperties.value(QStringLiteral("Metadata")));

    const QString title = unwrapMprisVariant(metadata.value(QStringLiteral("xesam:title"))).toString().trimmed();
    const QStringList artistList = variantToStringList(metadata.value(QStringLiteral("xesam:artist")));
    const QString artist = artistList.join(QStringLiteral(", "));
    const QString artSource = unwrapMprisVariant(metadata.value(QStringLiteral("mpris:artUrl"))).toString().trimmed();

    m_mprisTrackLengthUs = qMax<qint64>(0, unwrapMprisVariant(metadata.value(QStringLiteral("mpris:length"))).toLongLong());
    m_mprisPositionUs = mprisPlayerPositionUs(serviceName);

    if (m_mprisTrackLengthUs > 0)
        m_mprisPositionUs = qMin(m_mprisPositionUs, m_mprisTrackLengthUs);

    m_mprisLastPositionUs = m_mprisPositionUs;
    m_mprisLastPositionEpochMs = QDateTime::currentMSecsSinceEpoch();

    QVariantList sources;
    sources.reserve(services.size());
    for (const QString &service : services)
    {
        const QVariantMap properties = mprisPlayerProperties(service);
        if (properties.isEmpty())
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("serviceName"), service);
        entry.insert(QStringLiteral("selected"), service == m_mprisServiceName);
        entry.insert(QStringLiteral("playing"), unwrapMprisVariant(properties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed().compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0);

        QVariantMap sourceMetadata = mprisPlayerMetadata(service);
        if (sourceMetadata.isEmpty())
            sourceMetadata = variantToVariantMap(properties.value(QStringLiteral("Metadata")));

        const QString sourceTitle = unwrapMprisVariant(sourceMetadata.value(QStringLiteral("xesam:title"))).toString().trimmed();
        const QStringList sourceArtistList = variantToStringList(sourceMetadata.value(QStringLiteral("xesam:artist")));
        const QString sourceArtist = sourceArtistList.join(QStringLiteral(", "));
        const QString sourceLabel = service.mid(QString::fromLatin1(kMprisServicePrefix).length());
        const QString displayTitle = sourceTitle.isEmpty() ? sourceLabel : sourceTitle;

        entry.insert(QStringLiteral("title"), displayTitle);
        entry.insert(QStringLiteral("artist"), sourceArtist);
        entry.insert(QStringLiteral("subtitle"), sourceArtist.isEmpty() ? sourceLabel : sourceArtist);
        entry.insert(QStringLiteral("status"), unwrapMprisVariant(properties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed());
        entry.insert(QStringLiteral("artSource"), unwrapMprisVariant(sourceMetadata.value(QStringLiteral("mpris:artUrl"))).toString().trimmed());
        sources.append(entry);
    }
    setMprisSources(sources);

    const QString timestamp = formatMprisTimestamp(m_mprisPositionUs, m_mprisTrackLengthUs);
    setMediaTitle(title);
    setMediaArtist(artist);
    setMediaArtSource(artSource);
    setMediaTimestamp(timestamp);
    setMediaPlaying(isPlaying);
    setMprisVisible(true);
    updateMprisPlaybackTicker();
}

void ValenzBridge::mediaPreviousTrack()
{
    if (!invokeMprisPlayerMethod(QStringLiteral("Previous")))
        return;

    trace(QStringLiteral("mpris"), QStringLiteral("previous_track"));
    refreshMprisState();
}

void ValenzBridge::mediaTogglePlayPause()
{
    if (!invokeMprisPlayerMethod(QStringLiteral("PlayPause")))
        return;

    trace(QStringLiteral("mpris"), QStringLiteral("toggle_play_pause"));
    refreshMprisState();
}

void ValenzBridge::mediaNextTrack()
{
    if (!invokeMprisPlayerMethod(QStringLiteral("Next")))
        return;

    trace(QStringLiteral("mpris"), QStringLiteral("next_track"));
    refreshMprisState();
}

void ValenzBridge::selectMprisSource(const QString &serviceName)
{
    const QString trimmed = serviceName.trimmed();
    if (trimmed.isEmpty())
        return;

    if (m_mprisServiceName == trimmed)
        return;

    m_mprisServiceName = trimmed;
    refreshMprisState();
}

