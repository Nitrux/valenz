#include "valenzbridge.h"
#include "valenzbridge_p.h"

#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QMap>

namespace
{
using BluezPropertyMap = QMap<QString, QVariant>;
using BluezInterfaceMap = QMap<QString, BluezPropertyMap>;
using BluezManagedObjects = QMap<QDBusObjectPath, BluezInterfaceMap>;

void registerBluezMetaTypes()
{
    static bool registered = false;
    if (registered)
        return;

    qDBusRegisterMetaType<BluezPropertyMap>();
    qDBusRegisterMetaType<BluezInterfaceMap>();
    qDBusRegisterMetaType<BluezManagedObjects>();
    registered = true;
}

bool isBluezMediaSourceId(const QString &sourceId)
{
    return sourceId.startsWith(QLatin1Char('/'));
}

QString bluezParentDevicePath(const QString &playerPath)
{
    const int slashIndex = playerPath.lastIndexOf(QLatin1Char('/'));
    if (slashIndex <= 0)
        return {};

    return playerPath.left(slashIndex);
}

QString bluezFallbackDeviceLabel(const QString &devicePath)
{
    QString label = devicePath.section(QLatin1Char('/'), -1).trimmed();
    if (label.startsWith(QStringLiteral("dev_")))
    {
        label.remove(0, 4);
        label.replace(QLatin1Char('_'), QLatin1Char(':'));
        return label;
    }

    return label;
}

QString bluezDeviceLabel(const QString &playerPath)
{
    const QString devicePath = bluezParentDevicePath(playerPath);
    if (devicePath.isEmpty())
        return {};

    QDBusInterface propertiesIface(QString::fromLatin1(kBluezService),
                                   devicePath,
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::systemBus());

    if (propertiesIface.isValid())
    {
        const QDBusReply<QDBusVariant> aliasReply = propertiesIface.call(QStringLiteral("Get"),
                                                                          QString::fromLatin1(kBluezDeviceInterface),
                                                                          QStringLiteral("Alias"));
        if (aliasReply.isValid())
        {
            const QString alias = aliasReply.value().variant().toString().trimmed();
            if (!alias.isEmpty())
                return alias;
        }

        const QDBusReply<QDBusVariant> nameReply = propertiesIface.call(QStringLiteral("Get"),
                                                                        QString::fromLatin1(kBluezDeviceInterface),
                                                                        QStringLiteral("Name"));
        if (nameReply.isValid())
        {
            const QString name = nameReply.value().variant().toString().trimmed();
            if (!name.isEmpty())
                return name;
        }
    }

    return bluezFallbackDeviceLabel(devicePath);
}

QVariantMap bluezMediaPlayerProperties(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return {};

    QDBusInterface propertiesIface(QString::fromLatin1(kBluezService),
                                   objectPath,
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::systemBus());

    const QDBusReply<QVariantMap> reply = propertiesIface.call(QStringLiteral("GetAll"),
                                                                QString::fromLatin1(kBluezMediaPlayerInterface));
    if (!reply.isValid())
        return {};

    return reply.value();
}

QVariantMap bluezMediaPlayerTrack(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return {};

    QDBusInterface propertiesIface(QString::fromLatin1(kBluezService),
                                   objectPath,
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::systemBus());

    const QDBusReply<QDBusVariant> reply = propertiesIface.call(QStringLiteral("Get"),
                                                                QString::fromLatin1(kBluezMediaPlayerInterface),
                                                                QStringLiteral("Track"));
    if (!reply.isValid())
        return {};

    return variantToVariantMap(reply.value().variant());
}

qint64 bluezMediaPlayerPositionUs(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return 0;

    QDBusInterface propertiesIface(QString::fromLatin1(kBluezService),
                                   objectPath,
                                   QString::fromLatin1(kDbusPropertiesInterface),
                                   QDBusConnection::systemBus());

    const QDBusReply<QDBusVariant> reply = propertiesIface.call(QStringLiteral("Get"),
                                                                QString::fromLatin1(kBluezMediaPlayerInterface),
                                                                QStringLiteral("Position"));
    if (!reply.isValid())
        return 0;

    return qMax<qint64>(0, reply.value().variant().toLongLong() * 1000);
}

qint64 bluezTrackLengthUs(const QVariantMap &track)
{
    qint64 durationMs = unwrapMprisVariant(track.value(QStringLiteral("Duration"))).toLongLong();
    if (durationMs <= 0)
        durationMs = unwrapMprisVariant(track.value(QStringLiteral("Length"))).toLongLong();

    return qMax<qint64>(0, durationMs * 1000);
}

QString bluezSourceLabel(const QVariantMap &track, const QString &deviceLabel)
{
    const QString title = unwrapMprisVariant(track.value(QStringLiteral("Title"))).toString().trimmed();
    if (!title.isEmpty())
        return title;

    return deviceLabel;
}

QStringList bluezMediaPlayerObjectPaths()
{
    registerBluezMetaTypes();

    QDBusInterface objectsIface(QString::fromLatin1(kBluezService),
                                QStringLiteral("/"),
                                QString::fromLatin1(kBluezObjectManagerInterface),
                                QDBusConnection::systemBus());
    if (!objectsIface.isValid())
        return {};

    const QDBusReply<BluezManagedObjects> reply = objectsIface.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid())
        return {};

    QStringList objectPaths;
    const BluezManagedObjects objects = reply.value();
    for (const QDBusObjectPath &path : objects.keys())
    {
        const BluezInterfaceMap interfaces = objects.value(path);
        if (interfaces.contains(QString::fromLatin1(kBluezMediaPlayerInterface)))
            objectPaths << path.path();
    }

    return objectPaths;
}

bool invokeBluezMediaPlayerMethod(const QString &objectPath, const QString &method)
{
    if (objectPath.isEmpty() || method.isEmpty())
        return false;

    QDBusInterface playerIface(QString::fromLatin1(kBluezService),
                               objectPath,
                               QString::fromLatin1(kBluezMediaPlayerInterface),
                               QDBusConnection::systemBus());
    if (!playerIface.isValid())
        return false;

    const QDBusMessage result = playerIface.call(method);
    return result.type() != QDBusMessage::ErrorMessage;
}
}

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

    if (serviceName.isEmpty() || isBluezMediaSourceId(serviceName))
        return;

    const bool connected = QDBusConnection::sessionBus().connect(serviceName,
                                                                 QString::fromLatin1(kMprisObjectPath),
                                                                 QString::fromLatin1(kDbusPropertiesInterface),
                                                                 QStringLiteral("PropertiesChanged"),
                                                                 this,
                                                                 SLOT(onMprisPropertiesChanged(QString,QVariantMap,QStringList)));
    if (!connected)
        return;

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
    updateMprisRefreshTimer();
}

void ValenzBridge::updateMprisRefreshTimer()
{
    if (!m_mprisRefreshTimer)
        return;

    const bool needsPolling = !m_mprisServiceName.isEmpty() && isBluezMediaSourceId(m_mprisServiceName);

    if (needsPolling)
    {
        if (!m_mprisRefreshTimer->isActive())
            m_mprisRefreshTimer->start();
    }
    else if (m_mprisRefreshTimer->isActive())
    {
        m_mprisRefreshTimer->stop();
    }
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
    Q_UNUSED(changedProperties)
    Q_UNUSED(invalidatedProperties)

    if (interfaceName != QString::fromLatin1(kMprisPlayerInterface))
        return;

    refreshMprisState();
}

void ValenzBridge::onMprisNameOwnerChanged(const QString &serviceName,
                                           const QString &oldOwner,
                                           const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)

    if (!serviceName.startsWith(QString::fromLatin1(kMprisServicePrefix)))
        return;

    refreshMprisState();
}

QStringList ValenzBridge::mprisServiceNames() const
{
    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    if (!busInterface)
        return {};

    const QDBusReply<QStringList> namesReply = busInterface->registeredServiceNames();
    if (!namesReply.isValid())
        return {};

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
        return {};

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
        return {};

    return variantToVariantMap(reply.value().variant());
}

QString ValenzBridge::preferredMprisService() const
{
    const QStringList services = mprisServiceNames();
    const QStringList bluezPlayers = bluezMediaPlayerObjectPaths();

    if (services.isEmpty() && bluezPlayers.isEmpty())
        return {};

    if (!m_mprisServiceName.isEmpty())
    {
        if (services.contains(m_mprisServiceName) || bluezPlayers.contains(m_mprisServiceName))
            return m_mprisServiceName;
    }

    QString fallbackService = services.isEmpty() ? QString() : services.constFirst();
    for (const QString &service : services)
    {
        const QVariantMap properties = mprisPlayerProperties(service);
        const QString playbackStatus = unwrapMprisVariant(properties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed();
        if (playbackStatus.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0)
            return service;

        if (fallbackService.isEmpty() && !properties.isEmpty())
            fallbackService = service;
    }

    for (const QString &objectPath : bluezPlayers)
    {
        const QVariantMap properties = bluezMediaPlayerProperties(objectPath);
        const QString status = unwrapMprisVariant(properties.value(QStringLiteral("Status"))).toString().trimmed();
        if (status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0)
            return objectPath;
    }

    if (!fallbackService.isEmpty())
        return fallbackService;

    return bluezPlayers.constFirst();
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

    if (isBluezMediaSourceId(m_mprisServiceName))
        return invokeBluezMediaPlayerMethod(m_mprisServiceName, method);

    QDBusInterface playerIface(m_mprisServiceName,
                               QString::fromLatin1(kMprisObjectPath),
                               QString::fromLatin1(kMprisPlayerInterface),
                               QDBusConnection::sessionBus());
    const QDBusMessage result = playerIface.call(method);
    if (result.type() == QDBusMessage::ErrorMessage)
        return false;

    return true;
}

void ValenzBridge::refreshMprisState()
{
    const QStringList services = mprisServiceNames();
    const QStringList bluezPlayers = bluezMediaPlayerObjectPaths();
    QStringList sourceIds = services;
    sourceIds.append(bluezPlayers);

    if (sourceIds.isEmpty())
    {
        clearMprisPropertiesSubscription();
        clearMprisState();
        return;
    }

    QString sourceId = m_mprisServiceName;
    if (sourceId.isEmpty() || !sourceIds.contains(sourceId))
        sourceId = preferredMprisService();

    if (sourceId.isEmpty())
    {
        clearMprisPropertiesSubscription();
        clearMprisState();
        return;
    }

    const bool bluezSource = isBluezMediaSourceId(sourceId);
    if (bluezSource)
        clearMprisPropertiesSubscription();
    else
        updateMprisPropertiesSubscription(sourceId);

    QVariantMap selectedProperties;
    QVariantMap selectedMetadata;
    QString selectedTitle;
    QString selectedArtist;
    QString selectedArtSource;
    QString selectedPlaybackStatus;
    qint64 selectedPositionUs = 0;
    qint64 selectedTrackLengthUs = 0;
    bool selectedPlaying = false;

    if (bluezSource)
    {
        selectedProperties = bluezMediaPlayerProperties(sourceId);
        if (selectedProperties.isEmpty())
        {
            clearMprisState();
            return;
        }

        selectedPlaybackStatus = unwrapMprisVariant(selectedProperties.value(QStringLiteral("Status"))).toString().trimmed();
        selectedPlaying = selectedPlaybackStatus.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0;

        selectedMetadata = bluezMediaPlayerTrack(sourceId);
        const QString deviceLabel = bluezDeviceLabel(sourceId);
        const QString artistString = unwrapMprisVariant(selectedMetadata.value(QStringLiteral("Artist"))).toString().trimmed();
        const QStringList artistList = variantToStringList(selectedMetadata.value(QStringLiteral("Artist")));

        selectedTitle = bluezSourceLabel(selectedMetadata, deviceLabel);
        selectedArtist = !artistString.isEmpty() ? artistString : artistList.join(QStringLiteral(", "));
        selectedArtSource = QString();
        selectedTrackLengthUs = bluezTrackLengthUs(selectedMetadata);
        selectedPositionUs = bluezMediaPlayerPositionUs(sourceId);
    }
    else
    {
        selectedProperties = mprisPlayerProperties(sourceId);
        if (selectedProperties.isEmpty())
        {
            clearMprisState();
            return;
        }

        selectedPlaybackStatus = unwrapMprisVariant(selectedProperties.value(QStringLiteral("PlaybackStatus"))).toString().trimmed();
        selectedPlaying = selectedPlaybackStatus.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0;

        selectedMetadata = mprisPlayerMetadata(sourceId);
        if (selectedMetadata.isEmpty())
            selectedMetadata = variantToVariantMap(selectedProperties.value(QStringLiteral("Metadata")));

        selectedTitle = unwrapMprisVariant(selectedMetadata.value(QStringLiteral("xesam:title"))).toString().trimmed();
        selectedArtist = variantToStringList(selectedMetadata.value(QStringLiteral("xesam:artist"))).join(QStringLiteral(", "));
        selectedArtSource = unwrapMprisVariant(selectedMetadata.value(QStringLiteral("mpris:artUrl"))).toString().trimmed();
        selectedTrackLengthUs = qMax<qint64>(0, unwrapMprisVariant(selectedMetadata.value(QStringLiteral("mpris:length"))).toLongLong());
        selectedPositionUs = mprisPlayerPositionUs(sourceId);
    }

    if (selectedTrackLengthUs > 0)
        selectedPositionUs = qMin(selectedPositionUs, selectedTrackLengthUs);

    m_mprisServiceName = sourceId;
    m_mprisTrackLengthUs = selectedTrackLengthUs;
    m_mprisPositionUs = selectedPositionUs;
    m_mprisLastPositionUs = m_mprisPositionUs;
    m_mprisLastPositionEpochMs = QDateTime::currentMSecsSinceEpoch();

    QVariantList sources;
    sources.reserve(sourceIds.size());

    for (const QString &service : services)
    {
        const QVariantMap properties = mprisPlayerProperties(service);
        if (properties.isEmpty())
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("backend"), QStringLiteral("mpris"));
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

    for (const QString &objectPath : bluezPlayers)
    {
        const QVariantMap properties = bluezMediaPlayerProperties(objectPath);
        if (properties.isEmpty())
            continue;

        const QVariantMap track = bluezMediaPlayerTrack(objectPath);
        const QString deviceLabel = bluezDeviceLabel(objectPath);
        const QString trackTitle = bluezSourceLabel(track, deviceLabel);
        const QString artist = variantToStringList(track.value(QStringLiteral("Artist"))).join(QStringLiteral(", "));
        const QString status = unwrapMprisVariant(properties.value(QStringLiteral("Status"))).toString().trimmed();

        QVariantMap entry;
        entry.insert(QStringLiteral("backend"), QStringLiteral("bluez"));
        entry.insert(QStringLiteral("serviceName"), objectPath);
        entry.insert(QStringLiteral("selected"), objectPath == m_mprisServiceName);
        entry.insert(QStringLiteral("playing"), status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0);
        entry.insert(QStringLiteral("title"), trackTitle);
        entry.insert(QStringLiteral("artist"), artist);
        entry.insert(QStringLiteral("subtitle"), deviceLabel);
        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("artSource"), QString());
        sources.append(entry);
    }

    setMprisSources(sources);

    const QString timestamp = formatMprisTimestamp(m_mprisPositionUs, m_mprisTrackLengthUs);
    setMediaTitle(selectedTitle);
    setMediaArtist(selectedArtist);
    setMediaArtSource(selectedArtSource);
    setMediaTimestamp(timestamp);
    setMediaPlaying(selectedPlaying);
    setMprisVisible(true);
    updateMprisPlaybackTicker();
    updateMprisRefreshTimer();
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
    if (m_mprisServiceName.isEmpty())
        refreshMprisState();

    if (m_mprisServiceName.isEmpty())
        return;

    if (isBluezMediaSourceId(m_mprisServiceName))
    {
        const QString method = m_mediaPlaying ? QStringLiteral("Pause") : QStringLiteral("Play");
        if (!invokeBluezMediaPlayerMethod(m_mprisServiceName, method))
            return;
    }
    else
    {
        if (!invokeMprisPlayerMethod(QStringLiteral("PlayPause")))
            return;
    }

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
