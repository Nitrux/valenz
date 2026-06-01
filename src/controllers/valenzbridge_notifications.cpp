#include "valenzbridge_notifications.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QtGlobal>

namespace
{
constexpr auto kNotificationService = "org.freedesktop.Notifications";
constexpr auto kNotificationPath = "/org/freedesktop/Notifications";
constexpr auto kFallbackIcon = "notifications";

constexpr uint kCloseReasonExpired = 1;
constexpr uint kCloseReasonDismissedByUser = 2;
constexpr uint kCloseReasonClosedByCall = 3;
}

NotificationsController::NotificationsController(QObject *parent)
    : QAbstractListModel(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    const bool objectRegistered = bus.registerObject(QString::fromLatin1(kNotificationPath),
                                                     QStringLiteral("org.freedesktop.Notifications"),
                                                     this,
                                                     QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);

    const bool serviceRegistered = bus.registerService(QString::fromLatin1(kNotificationService));
    setAvailable(objectRegistered && serviceRegistered);
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

    for (uint id : ids)
        Q_EMIT NotificationClosed(id, kCloseReasonClosedByCall);
}

void NotificationsController::dismiss(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    removeByIndex(index, kCloseReasonDismissedByUser);
}

void NotificationsController::invokeAction(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    const NotificationEntry &entry = m_entries.at(index);
    const QString actionKey = entry.actionKey.isEmpty() ? QStringLiteral("default") : entry.actionKey;
    Q_EMIT ActionInvoked(entry.id, actionKey);

    removeByIndex(index, kCloseReasonDismissedByUser);
}

void NotificationsController::refreshTimestamps()
{
    if (m_entries.isEmpty())
        return;

    Q_EMIT dataChanged(this->index(0, 0), this->index(m_entries.size() - 1, 0), {TimestampTextRole});
}

void NotificationsController::setDndEnabled(bool enabled)
{
    if (m_dndEnabled == enabled)
        return;

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

    if (m_dndEnabled)
        return 0;

    NotificationEntry entry;
    entry.id = replacesId > 0 ? replacesId : m_nextId++;
    entry.sourceName = appName.trimmed().isEmpty() ? QStringLiteral("Notification") : appName.trimmed();

    const QString cleanSummary = summary.trimmed();
    const QString cleanBody = body.trimmed();
    if (!cleanSummary.isEmpty() && !cleanBody.isEmpty())
        entry.messageText = QStringLiteral("%1\n%2").arg(cleanSummary, cleanBody);
    else if (!cleanBody.isEmpty())
        entry.messageText = cleanBody;
    else if (!cleanSummary.isEmpty())
        entry.messageText = cleanSummary;
    else
        entry.messageText = QStringLiteral("(No details)");

    entry.createdAt = QDateTime::currentDateTime();
    entry.iconName = normalizeIconName(appIcon, hints);
    entry.urgencyLevel = parseUrgency(hints);
    entry.actionText = chooseActionText(actions);
    entry.actionKey = chooseActionKey(actions);

    const int replaceRow = replacesId > 0 ? indexOfId(replacesId) : -1;
    if (replaceRow >= 0)
    {
        m_entries[replaceRow] = entry;
        Q_EMIT dataChanged(index(replaceRow, 0), index(replaceRow, 0));
        Q_EMIT transientNotification(entry.sourceName, entry.messageText, entry.iconName, entry.urgencyLevel);
        return entry.id;
    }

    const int insertRow = m_entries.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_entries.push_back(entry);
    endInsertRows();
    Q_EMIT countChanged(m_entries.size());
    Q_EMIT transientNotification(entry.sourceName, entry.messageText, entry.iconName, entry.urgencyLevel);
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

    return QStringLiteral("Open");
}

QString NotificationsController::chooseActionKey(const QStringList &actions)
{
    if (!actions.isEmpty())
        return actions.at(0).trimmed();

    return QStringLiteral("default");
}

QString NotificationsController::normalizeIconName(const QString &iconName, const QVariantMap &hints)
{
    const QString explicitIcon = iconName.trimmed();
    if (!explicitIcon.isEmpty())
        return explicitIcon;

    const QString hintedIcon = hints.value(QStringLiteral("image-path")).toString().trimmed();
    if (!hintedIcon.isEmpty())
        return hintedIcon;

    return QString::fromLatin1(kFallbackIcon);
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
    Q_EMIT NotificationClosed(id, closeReason);
}

void NotificationsController::setAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    Q_EMIT availableChanged(m_available);
}
