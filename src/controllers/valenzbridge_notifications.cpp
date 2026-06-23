#include "valenzbridge_notifications.h"
#include "valenzbridge_p.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QtGlobal>

namespace
{
constexpr auto kNotificationService = "org.freedesktop.Notifications";
constexpr auto kNotificationPath = "/org/freedesktop/Notifications";
constexpr auto kFallbackIcon = "notifications";

constexpr uint kCloseReasonExpired = 1;
constexpr uint kCloseReasonDismissedByUser = 2;
constexpr uint kCloseReasonClosedByCall = 3;

void emitNotificationsSignal(const QString &member, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createSignal(QString::fromLatin1(kNotificationPath),
                                                      QString::fromLatin1(kNotificationService),
                                                      member);
    message.setArguments(arguments);
    QDBusConnection::sessionBus().send(message);
}

QString normalizeNotificationText(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QStringLiteral("\r"), QStringLiteral("\n"));

    const QStringList lines = normalized.split(QStringLiteral("\n"));
    QStringList paragraphs;
    QStringList paragraphLines;

    for (const QString &line : lines)
    {
        const QString compactLine = line.simplified();
        if (compactLine.isEmpty())
        {
            if (!paragraphLines.isEmpty())
            {
                paragraphs.push_back(paragraphLines.join(QStringLiteral(" ")));
                paragraphLines.clear();
            }
            continue;
        }

        paragraphLines.push_back(compactLine);
    }

    if (!paragraphLines.isEmpty())
        paragraphs.push_back(paragraphLines.join(QStringLiteral(" ")));

    return paragraphs.join(QStringLiteral("\n\n")).trimmed();
}
}

NotificationsController::NotificationsController(QObject *parent)
    : QAbstractListModel(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    const bool objectRegistered = bus.registerObject(QString::fromLatin1(kNotificationPath),
                                                     QStringLiteral("org.freedesktop.Notifications"),
                                                     this,
                                                     QDBusConnection::ExportAllSlots);

    const bool serviceRegistered = bus.registerService(QString::fromLatin1(kNotificationService));
    setAvailable(objectRegistered && serviceRegistered);

    auto *timestampTimer = new QTimer(this);
    timestampTimer->setTimerType(Qt::CoarseTimer);
    timestampTimer->setInterval(15000);
    connect(timestampTimer, &QTimer::timeout, this, &NotificationsController::refreshTimestamps);
    timestampTimer->start();
}

int NotificationsController::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_entries.size();
}

QVariant NotificationsController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const NotificationEntry &entry = m_entries.at(index.row());

    switch (role)
    {
    case IdRole:
        return static_cast<int>(entry.id);
    case SourceNameRole:
        return entry.sourceName;
    case MessageTextRole:
        return entry.messageText;
    case TimestampTextRole:
        return relativeTimestamp(entry.createdAt);
    case IconNameRole:
        return entry.iconName;
    case UrgencyLevelRole:
        return entry.urgencyLevel;
    case ActionTextRole:
        return entry.actionText;
    case ActionKeyRole:
        return entry.actionKey;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationsController::roleNames() const
{
    return {
        {IdRole, "id"},
        {SourceNameRole, "sourceName"},
        {MessageTextRole, "messageText"},
        {TimestampTextRole, "timestampText"},
        {IconNameRole, "iconName"},
        {UrgencyLevelRole, "urgencyLevel"},
        {ActionTextRole, "actionText"},
        {ActionKeyRole, "actionKey"},
    };
}

int NotificationsController::count() const
{
    return m_entries.size();
}

bool NotificationsController::dndEnabled() const
{
    return m_dndEnabled;
}

bool NotificationsController::available() const
{
    return m_available;
}

QVariantList NotificationsController::groupedNotifications() const
{
    struct GroupData
    {
        QString key;
        QString sourceName;
        QVector<NotificationEntry> entries;
    };

    QVector<GroupData> groups;
    groups.reserve(m_entries.size());
    QHash<QString, int> groupIndexes;

    for (const NotificationEntry &entry : m_entries)
    {
        const QString key = normalizedGroupKey(entry.sourceName);
        const int groupIndex = groupIndexes.value(key, -1);

        if (groupIndex < 0)
        {
            GroupData group;
            group.key = key;
            group.sourceName = entry.sourceName;
            group.entries.push_back(entry);
            groups.push_back(group);
            groupIndexes.insert(key, groups.size() - 1);
            continue;
        }

        groups[groupIndex].entries.push_back(entry);
    }

    QVariantList result;
    result.reserve(groups.size());

    for (const GroupData &group : std::as_const(groups))
    {
        QVariantMap groupMap;
        groupMap.insert(QStringLiteral("key"), group.key);
        groupMap.insert(QStringLiteral("sourceName"), group.sourceName);
        groupMap.insert(QStringLiteral("count"), static_cast<int>(group.entries.size()));

        if (!group.entries.isEmpty())
        {
            const NotificationEntry &latest = group.entries.first();
            groupMap.insert(QStringLiteral("latestId"), static_cast<int>(latest.id));
            groupMap.insert(QStringLiteral("latestMessageText"), latest.messageText);
            groupMap.insert(QStringLiteral("latestTimestampText"), relativeTimestamp(latest.createdAt));
            groupMap.insert(QStringLiteral("latestIconName"), latest.iconName);
            groupMap.insert(QStringLiteral("latestUrgencyLevel"), latest.urgencyLevel);
            groupMap.insert(QStringLiteral("latestActionText"), latest.actionText);
            groupMap.insert(QStringLiteral("latestActionKey"), latest.actionKey);
        }
        else
        {
            groupMap.insert(QStringLiteral("latestId"), 0);
            groupMap.insert(QStringLiteral("latestMessageText"), QString());
            groupMap.insert(QStringLiteral("latestTimestampText"), QStringLiteral("Now"));
            groupMap.insert(QStringLiteral("latestIconName"), QString::fromLatin1(kFallbackIcon));
            groupMap.insert(QStringLiteral("latestUrgencyLevel"), 0);
            groupMap.insert(QStringLiteral("latestActionText"), QString());
            groupMap.insert(QStringLiteral("latestActionKey"), QString());
        }

        QVariantList notifications;
        notifications.reserve(group.entries.size());
        for (const NotificationEntry &entry : group.entries)
            notifications.push_back(notificationEntryToMap(entry));
        groupMap.insert(QStringLiteral("notifications"), notifications);

        result.push_back(groupMap);
    }

    return result;
}

void NotificationsController::clearAllNotifications()
{
    if (m_entries.isEmpty())
        return;

    QVector<uint> ids;
    ids.reserve(m_entries.size());
    for (const NotificationEntry &entry : m_entries)
        ids.push_back(entry.id);

    beginResetModel();
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged(0);
    Q_EMIT notificationsChanged();

    for (uint id : ids)
    {
        Q_EMIT NotificationClosed(id, kCloseReasonClosedByCall);
        emitNotificationsSignal(QStringLiteral("NotificationClosed"),
                                {QVariant::fromValue(id), QVariant::fromValue(kCloseReasonClosedByCall)});
    }
}

void NotificationsController::dismiss(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    removeByIndex(index, kCloseReasonDismissedByUser);
}

void NotificationsController::dismissById(uint id)
{
    const int row = indexOfId(id);
    if (row < 0)
        return;

    removeByIndex(row, kCloseReasonDismissedByUser);
}

void NotificationsController::dismissGroup(const QString &sourceName)
{
    const QString groupKey = normalizedGroupKey(sourceName);
    bool removedAny = false;

    for (int row = m_entries.size() - 1; row >= 0; --row)
    {
        if (normalizedGroupKey(m_entries.at(row).sourceName) != groupKey)
            continue;

        removeByIndex(row, kCloseReasonDismissedByUser);
        removedAny = true;
    }

    if (removedAny)
        Q_EMIT notificationsChanged();
}

void NotificationsController::invokeAction(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    const NotificationEntry &entry = m_entries.at(index);
    if (entry.actionKey.isEmpty())
        return;

    Q_EMIT ActionInvoked(entry.id, entry.actionKey);
    emitNotificationsSignal(QStringLiteral("ActionInvoked"),
                            {QVariant::fromValue(entry.id), QVariant::fromValue(entry.actionKey)});

    removeByIndex(index, kCloseReasonDismissedByUser);
}

void NotificationsController::invokeActionById(uint id)
{
    const int row = indexOfId(id);
    if (row < 0)
        return;

    invokeAction(row);
}

void NotificationsController::refreshTimestamps()
{
    if (m_entries.isEmpty())
        return;

    Q_EMIT dataChanged(this->index(0, 0), this->index(m_entries.size() - 1, 0), {TimestampTextRole});
    Q_EMIT notificationsChanged();
}

void NotificationsController::setDndEnabled(bool enabled)
{
    if (m_dndEnabled == enabled)
    {
        return;
    }
    m_dndEnabled = enabled;
    Q_EMIT dndEnabledChanged(m_dndEnabled);
}

void NotificationsController::toggleDnd()
{
    setDndEnabled(!m_dndEnabled);
}

uint NotificationsController::Notify(const QString &appName,
                                     uint replacesId,
                                     const QString &appIcon,
                                     const QString &summary,
                                     const QString &body,
                                     const QStringList &actions,
                                     const QVariantMap &hints,
                                     int timeout)
{
    Q_UNUSED(timeout)

    NotificationEntry entry;
    entry.id = replacesId > 0 ? replacesId : m_nextId++;
    entry.sourceName = appName.trimmed().isEmpty() ? QStringLiteral("Notification") : appName.trimmed();

    const QString cleanSummary = normalizeNotificationText(summary);
    const QString cleanBody = normalizeNotificationText(body);
    if (!cleanSummary.isEmpty() && !cleanBody.isEmpty())
        entry.messageText = QStringLiteral("%1\n\n%2").arg(cleanSummary, cleanBody);
    else if (!cleanBody.isEmpty())
        entry.messageText = cleanBody;
    else if (!cleanSummary.isEmpty())
        entry.messageText = cleanSummary;
    else
        entry.messageText = QStringLiteral("(No details)");

    entry.createdAt = QDateTime::currentDateTime();
    entry.iconName = normalizeIconName(appIcon, appName, hints);
    entry.urgencyLevel = parseUrgency(hints);
    entry.actionText = chooseActionText(actions);
    entry.actionKey = chooseActionKey(actions);

    const int replaceRow = replacesId > 0 ? indexOfId(replacesId) : -1;
    const bool suppressTransient = m_dndEnabled;

    if (replaceRow >= 0)
    {
        m_entries[replaceRow] = entry;
        Q_EMIT dataChanged(index(replaceRow, 0), index(replaceRow, 0));
        Q_EMIT notificationsChanged();
        if (!suppressTransient)
            Q_EMIT transientNotification(entry.id, entry.sourceName, entry.messageText, relativeTimestamp(entry.createdAt), entry.iconName, entry.urgencyLevel, entry.actionText, entry.actionKey);
        return entry.id;
    }

    const int insertRow = 0;
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_entries.prepend(entry);
    endInsertRows();
    Q_EMIT countChanged(m_entries.size());
    Q_EMIT notificationsChanged();
    if (!suppressTransient)
        Q_EMIT transientNotification(entry.id, entry.sourceName, entry.messageText, relativeTimestamp(entry.createdAt), entry.iconName, entry.urgencyLevel, entry.actionText, entry.actionKey);
    return entry.id;
}

void NotificationsController::CloseNotification(uint id)
{
    const int row = indexOfId(id);
    if (row < 0)
        return;

    removeByIndex(row, kCloseReasonClosedByCall);
}

QStringList NotificationsController::GetCapabilities() const
{
    return {
        QStringLiteral("body"),
        QStringLiteral("actions"),
        QStringLiteral("persistence"),
        QStringLiteral("icon-static")
    };
}

QString NotificationsController::GetServerInformation(QString &vendor, QString &version, QString &specVersion) const
{
    vendor = QStringLiteral("Nitrux");
    version = QStringLiteral("0.1");
    specVersion = QStringLiteral("1.2");
    return QStringLiteral("Valenz");
}

int NotificationsController::indexOfId(uint id) const
{
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries.at(i).id == id)
            return i;
    }

    return -1;
}

QString NotificationsController::relativeTimestamp(const QDateTime &createdAt) const
{
    if (!createdAt.isValid())
        return QStringLiteral("Now");

    const qint64 seconds = createdAt.secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        return QStringLiteral("Now");

    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QStringLiteral("%1 min ago").arg(minutes);

    const qint64 hours = minutes / 60;
    if (hours < 24)
        return QStringLiteral("%1 h ago").arg(hours);

    const qint64 days = hours / 24;
    return QStringLiteral("%1 d ago").arg(days);
}

QString NotificationsController::normalizedGroupKey(const QString &sourceName)
{
    QString normalized = sourceName.simplified().trimmed().toLower();
    if (normalized.isEmpty())
        normalized = QStringLiteral("notification");

    return normalized;
}

int NotificationsController::parseUrgency(const QVariantMap &hints)
{
    const QVariant urgency = hints.value(QStringLiteral("urgency"));
    bool ok = false;
    int parsed = urgency.toInt(&ok);
    if (!ok)
        return 0;

    return qBound(0, parsed, 2);
}

QString NotificationsController::chooseActionText(const QStringList &actions)
{
    if (actions.size() >= 2)
        return actions.at(1).trimmed();

    return QString();
}

QString NotificationsController::chooseActionKey(const QStringList &actions)
{
    if (actions.size() >= 1)
        return actions.at(0).trimmed();

    return QString();
}

QString NotificationsController::normalizeIconName(const QString &iconName, const QString &appName, const QVariantMap &hints)
{
    const QString hintedIcon = hints.value(QStringLiteral("image-path")).toString().trimmed();
    if (!hintedIcon.isEmpty())
        return hintedIcon;

    const QString hintedIconAlt = hints.value(QStringLiteral("image_path")).toString().trimmed();
    if (!hintedIconAlt.isEmpty())
        return hintedIconAlt;

    QStringList iconCandidates;
    addWindowIconCandidates(&iconCandidates, iconName);
    addWindowIconCandidates(&iconCandidates, appName);
    addWindowIconCandidates(&iconCandidates, hints.value(QStringLiteral("desktop-entry")).toString());
    addWindowIconCandidates(&iconCandidates, hints.value(QStringLiteral("desktop_entry")).toString());

    for (const QString &candidate : std::as_const(iconCandidates))
    {
        if (isUsableIconSource(candidate))
            return candidate.trimmed();
    }

    for (const QString &candidate : std::as_const(iconCandidates))
    {
        const QString mappedIcon = lookupIconFromDesktopEntries(candidate);
        if (mappedIcon.isEmpty())
            continue;

        if (isUsableIconSource(mappedIcon))
            return mappedIcon.trimmed();

        return mappedIcon.trimmed();
    }

    const QString explicitIcon = iconName.trimmed();
    if (!explicitIcon.isEmpty())
        return explicitIcon;

    return QString::fromLatin1(kFallbackIcon);
}

QVariantMap NotificationsController::notificationEntryToMap(const NotificationEntry &entry) const
{
    QVariantMap result;
    result.insert(QStringLiteral("id"), static_cast<int>(entry.id));
    result.insert(QStringLiteral("sourceName"), entry.sourceName);
    result.insert(QStringLiteral("messageText"), entry.messageText);
    result.insert(QStringLiteral("timestampText"), relativeTimestamp(entry.createdAt));
    result.insert(QStringLiteral("iconName"), entry.iconName);
    result.insert(QStringLiteral("urgencyLevel"), entry.urgencyLevel);
    result.insert(QStringLiteral("actionText"), entry.actionText);
    result.insert(QStringLiteral("actionKey"), entry.actionKey);
    return result;
}

void NotificationsController::removeByIndex(int row, uint closeReason)
{
    if (row < 0 || row >= m_entries.size())
        return;

    const uint id = m_entries.at(row).id;

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();

    Q_EMIT countChanged(m_entries.size());
    Q_EMIT notificationsChanged();
    Q_EMIT NotificationClosed(id, closeReason);
    emitNotificationsSignal(QStringLiteral("NotificationClosed"),
                            {QVariant::fromValue(id), QVariant::fromValue(closeReason)});
}

void NotificationsController::setAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    Q_EMIT availableChanged(m_available);
}
