#include "valenzbridge_systray.h"
#include "valenzbridge_p.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>
#include <QUrl>

namespace
{
constexpr auto kWatcherPath = "/StatusNotifierWatcher";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto kDefaultItemPath = "/StatusNotifierItem";

const QStringList kWatcherServices {
    QStringLiteral("org.kde.StatusNotifierWatcher"),
    QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
};

const QStringList kItemInterfaces {
    QStringLiteral("org.kde.StatusNotifierItem"),
    QStringLiteral("org.freedesktop.StatusNotifierItem"),
};

QString shortServiceName(const QString &service)
{
    const QString trimmed = service.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(':')))
        return {};

    return trimmed.section(QLatin1Char('.'), -1);
}

bool resolveUsableIcon(const QString &candidate, QString *resolvedName, QString *resolvedSource)
{
    if (!resolvedName || !resolvedSource)
        return false;

    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (QFileInfo::exists(trimmed))
    {
        *resolvedName = QString();
        *resolvedSource = QUrl::fromLocalFile(trimmed).toString();
        return true;
    }

    if (QIcon::hasThemeIcon(trimmed))
    {
        *resolvedName = trimmed;
        resolvedSource->clear();
        return true;
    }

    return false;
}
}

SystemTrayController::SystemTrayController(QObject *parent)
    : QAbstractListModel(parent)
{
    m_watcherServiceWatcher = new QDBusServiceWatcher(this);
    m_watcherServiceWatcher->setConnection(QDBusConnection::sessionBus());
    m_watcherServiceWatcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    for (const QString &watcherService : kWatcherServices)
        m_watcherServiceWatcher->addWatchedService(watcherService);
    connect(m_watcherServiceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &SystemTrayController::onWatcherServiceOwnerChanged);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setTimerType(Qt::CoarseTimer);
    m_refreshTimer->setInterval(3000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SystemTrayController::refresh);
    m_refreshTimer->start();

    QTimer::singleShot(0, this, &SystemTrayController::refresh);
}

int SystemTrayController::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_items.size();
}

QVariant SystemTrayController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const TrayItemEntry &entry = m_items.at(index.row());

    switch (role)
    {
    case IdRole:
        return entry.id;
    case TitleRole:
        return entry.title;
    case IconNameRole:
        return effectiveIconName(entry);
    case IconSourceRole:
        return effectiveIconSource(entry);
    case StatusRole:
        return entry.status;
    case ServiceRole:
        return entry.service;
    case ObjectPathRole:
        return entry.objectPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> SystemTrayController::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {IconNameRole, "iconName"},
        {IconSourceRole, "iconSource"},
        {StatusRole, "status"},
        {ServiceRole, "service"},
        {ObjectPathRole, "objectPath"},
    };
}

int SystemTrayController::count() const
{
    return m_items.size();
}

bool SystemTrayController::available() const
{
    return m_available;
}

void SystemTrayController::activate(int index)
{
    requestItemMethod(index, QStringLiteral("Activate"));
}

void SystemTrayController::secondaryActivate(int index)
{
    requestItemMethod(index, QStringLiteral("SecondaryActivate"));
}

void SystemTrayController::contextMenu(int index)
{
    requestItemMethod(index, QStringLiteral("ContextMenu"));
}

void SystemTrayController::refresh()
{
    if (!ensureWatcher())
    {
        replaceAllItems({});
        return;
    }

    connectWatcherSignals();
    registerAsHost();

    const QVector<QString> itemIds = fetchRegisteredItemIds();
    QVector<TrayItemEntry> rebuilt;
    rebuilt.reserve(itemIds.size());

    for (const QString &itemId : itemIds)
    {
        const TrayItemEntry entry = buildItem(itemId);
        if (entry.service.isEmpty() || entry.objectPath.isEmpty())
            continue;

        rebuilt.push_back(entry);
    }

    replaceAllItems(rebuilt);
}

void SystemTrayController::onWatcherServiceOwnerChanged(const QString &service,
                                                        const QString &oldOwner,
                                                        const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)

    if (!kWatcherServices.contains(service))
        return;

    m_watcherSignalsConnected = false;
    m_hostRegistered = false;
    refresh();
}

void SystemTrayController::onStatusNotifierItemRegistered(const QString &itemId)
{
    if (itemId.trimmed().isEmpty())
        return;

    if (indexOfItemId(itemId) >= 0)
    {
        refresh();
        return;
    }

    const TrayItemEntry entry = buildItem(itemId);
    if (entry.service.isEmpty() || entry.objectPath.isEmpty())
        return;

    const int insertRow = m_items.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_items.push_back(entry);
    endInsertRows();
    Q_EMIT countChanged();
}

void SystemTrayController::onStatusNotifierItemUnregistered(const QString &itemId)
{
    const int row = indexOfItemId(itemId);
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

bool SystemTrayController::parseItemId(const QString &itemId, QString *service, QString *objectPath)
{
    if (!service || !objectPath)
        return false;

    const QString trimmed = itemId.trimmed();
    if (trimmed.isEmpty())
        return false;

    const int slashIndex = trimmed.indexOf(QLatin1Char('/'));
    if (slashIndex > 0)
    {
        *service = trimmed.left(slashIndex);
        *objectPath = trimmed.mid(slashIndex);
        return !service->isEmpty() && !objectPath->isEmpty();
    }

    if (trimmed.startsWith(QLatin1Char('/')))
        return false;

    *service = trimmed;
    *objectPath = QString::fromLatin1(kDefaultItemPath);
    return true;
}

QString SystemTrayController::effectiveIconName(const TrayItemEntry &entry)
{
    if (!entry.iconName.trimmed().isEmpty())
        return entry.iconName.trimmed();

    return QStringLiteral("application-x-executable");
}

QString SystemTrayController::effectiveIconSource(const TrayItemEntry &entry)
{
    return entry.iconSource.trimmed();
}

QString SystemTrayController::watcherService() const
{
    return m_watcherService;
}

QString SystemTrayController::watcherInterface() const
{
    if (m_watcherService == QStringLiteral("org.freedesktop.StatusNotifierWatcher"))
        return QStringLiteral("org.freedesktop.StatusNotifierWatcher");

    return QStringLiteral("org.kde.StatusNotifierWatcher");
}

bool SystemTrayController::ensureWatcher()
{
    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    if (!busInterface)
    {
        setAvailable(false);
        m_watcherService.clear();
        return false;
    }

    const QDBusReply<QStringList> namesReply = busInterface->registeredServiceNames();
    if (!namesReply.isValid())
    {
        setAvailable(false);
        m_watcherService.clear();
        return false;
    }

    const QStringList names = namesReply.value();
    QString resolvedService;
    for (const QString &candidate : kWatcherServices)
    {
        if (names.contains(candidate))
        {
            resolvedService = candidate;
            break;
        }
    }

    if (resolvedService != m_watcherService)
    {
        m_watcherService = resolvedService;
        m_watcherSignalsConnected = false;
        m_hostRegistered = false;
    }

    const bool hasWatcher = !m_watcherService.isEmpty();
    setAvailable(hasWatcher);
    return hasWatcher;
}

bool SystemTrayController::connectWatcherSignals()
{
    if (m_watcherSignalsConnected || m_watcherService.isEmpty())
        return m_watcherSignalsConnected;

    const QString watcherIface = watcherInterface();
    QDBusConnection bus = QDBusConnection::sessionBus();

    const bool regConnected = bus.connect(m_watcherService,
                                          QString::fromLatin1(kWatcherPath),
                                          watcherIface,
                                          QStringLiteral("StatusNotifierItemRegistered"),
                                          this,
                                          SLOT(onStatusNotifierItemRegistered(QString)));

    const bool unregConnected = bus.connect(m_watcherService,
                                            QString::fromLatin1(kWatcherPath),
                                            watcherIface,
                                            QStringLiteral("StatusNotifierItemUnregistered"),
                                            this,
                                            SLOT(onStatusNotifierItemUnregistered(QString)));

    m_watcherSignalsConnected = regConnected && unregConnected;
    return m_watcherSignalsConnected;
}

void SystemTrayController::registerAsHost()
{
    if (m_hostRegistered || m_watcherService.isEmpty())
        return;

    const QString hostService = QDBusConnection::sessionBus().baseService().trimmed();
    if (hostService.isEmpty())
        return;

    QDBusInterface watcherIface(m_watcherService,
                                QString::fromLatin1(kWatcherPath),
                                watcherInterface(),
                                QDBusConnection::sessionBus());

    if (!watcherIface.isValid())
        return;

    const QDBusReply<void> reply = watcherIface.call(QStringLiteral("RegisterStatusNotifierHost"), hostService);
    if (reply.isValid())
        m_hostRegistered = true;
}

QVector<QString> SystemTrayController::fetchRegisteredItemIds() const
{
    QVector<QString> ids;
    if (m_watcherService.isEmpty())
        return ids;

    QDBusInterface propertiesIface(m_watcherService,
                                   QString::fromLatin1(kWatcherPath),
                                   QString::fromLatin1(kPropertiesInterface),
                                   QDBusConnection::sessionBus());
    if (!propertiesIface.isValid())
        return ids;

    const QDBusReply<QDBusVariant> reply = propertiesIface.call(QStringLiteral("Get"),
                                                                watcherInterface(),
                                                                QStringLiteral("RegisteredStatusNotifierItems"));
    if (!reply.isValid())
        return ids;

    const QStringList rawIds = reply.value().variant().toStringList();
    ids.reserve(rawIds.size());

    for (const QString &rawId : rawIds)
    {
        const QString trimmed = rawId.trimmed();
        if (!trimmed.isEmpty())
            ids.push_back(trimmed);
    }

    return ids;
}

SystemTrayController::TrayItemEntry SystemTrayController::buildItem(const QString &itemId) const
{
    TrayItemEntry entry;
    entry.id = itemId.trimmed();

    if (!parseItemId(entry.id, &entry.service, &entry.objectPath))
        return entry;

    QVariantMap properties;
    for (const QString &itemInterface : kItemInterfaces)
    {
        properties = fetchItemProperties(entry.service, entry.objectPath, itemInterface);
        if (!properties.isEmpty())
            break;
    }

    entry.status = properties.value(QStringLiteral("Status")).toString().trimmed();
    entry.title = properties.value(QStringLiteral("Title")).toString().trimmed();
    const QString itemIdProperty = properties.value(QStringLiteral("Id")).toString().trimmed();

    if (entry.title.isEmpty())
        entry.title = itemIdProperty;
    if (entry.title.isEmpty())
        entry.title = entry.service;

    entry.iconName = properties.value(QStringLiteral("IconName")).toString().trimmed();
    entry.attentionIconName = properties.value(QStringLiteral("AttentionIconName")).toString().trimmed();

    const bool needsAttention = entry.status.compare(QStringLiteral("NeedsAttention"), Qt::CaseInsensitive) == 0;
    const QString preferredIcon = needsAttention && !entry.attentionIconName.isEmpty()
                                  ? entry.attentionIconName
                                  : entry.iconName;

    QStringList iconCandidates;
    addWindowIconCandidates(&iconCandidates, preferredIcon);
    addWindowIconCandidates(&iconCandidates, entry.iconName);
    addWindowIconCandidates(&iconCandidates, entry.attentionIconName);
    addWindowIconCandidates(&iconCandidates, itemIdProperty);
    addWindowIconCandidates(&iconCandidates, entry.title);
    addWindowIconCandidates(&iconCandidates, entry.service);
    addWindowIconCandidates(&iconCandidates, shortServiceName(entry.service));

    QString resolvedIconName = QStringLiteral("application-x-executable");
    QString resolvedIconSource;

    for (const QString &candidate : std::as_const(iconCandidates))
    {
        if (resolveUsableIcon(candidate, &resolvedIconName, &resolvedIconSource))
            break;
    }

    if (resolvedIconSource.isEmpty() && resolvedIconName == QLatin1String("application-x-executable"))
    {
        for (const QString &candidate : std::as_const(iconCandidates))
        {
            const QString mappedIcon = lookupIconFromDesktopEntries(candidate);
            if (mappedIcon.isEmpty())
                continue;

            if (resolveUsableIcon(mappedIcon, &resolvedIconName, &resolvedIconSource))
                break;

            if (resolvedIconName == QLatin1String("application-x-executable"))
                resolvedIconName = mappedIcon;
        }
    }

    if (resolvedIconSource.isEmpty() && resolvedIconName == QLatin1String("application-x-executable"))
    {
        if (!preferredIcon.isEmpty())
            resolvedIconName = preferredIcon;
    }

    entry.iconName = resolvedIconName;
    entry.iconSource = resolvedIconSource;

    return entry;
}

QVariantMap SystemTrayController::fetchItemProperties(const QString &service,
                                                      const QString &objectPath,
                                                      const QString &itemInterface) const
{
    if (service.isEmpty() || objectPath.isEmpty() || itemInterface.isEmpty())
        return {};

    QDBusInterface propertiesIface(service,
                                   objectPath,
                                   QString::fromLatin1(kPropertiesInterface),
                                   QDBusConnection::sessionBus());
    if (!propertiesIface.isValid())
        return {};

    const QDBusReply<QVariantMap> reply = propertiesIface.call(QStringLiteral("GetAll"), itemInterface);
    if (!reply.isValid())
        return {};

    return reply.value();
}

int SystemTrayController::indexOfItemId(const QString &itemId) const
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).id == itemId)
            return i;
    }

    return -1;
}

void SystemTrayController::replaceAllItems(const QVector<TrayItemEntry> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    Q_EMIT countChanged();
}

void SystemTrayController::requestItemMethod(int index, const QString &method)
{
    if (index < 0 || index >= m_items.size())
        return;

    const TrayItemEntry &entry = m_items.at(index);
    if (entry.service.isEmpty() || entry.objectPath.isEmpty() || method.trimmed().isEmpty())
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &itemInterface : kItemInterfaces)
    {
        QDBusInterface itemIface(entry.service, entry.objectPath, itemInterface, bus);
        if (!itemIface.isValid())
            continue;

        itemIface.call(method, 0, 0);
        break;
    }
}

void SystemTrayController::setAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    Q_EMIT availableChanged(m_available);
}
