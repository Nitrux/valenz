#include "valenzbridge_systray.h"
#include "valenzbridge_p.h"

#include <QDebug>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QtGlobal>
#include <QSettings>
#include <QTimer>
#include <QUrl>

struct DBusMenuLayoutItem
{
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuLayoutItem> children;
};

Q_DECLARE_METATYPE(DBusMenuLayoutItem)

namespace
{

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuLayoutItem &obj)
{
    argument.beginStructure();
    argument << obj.id << obj.properties;
    argument.beginArray(qMetaTypeId<QDBusVariant>());
    for (const DBusMenuLayoutItem &child : obj.children)
        argument << QDBusVariant(QVariant::fromValue(child));
    argument.endArray();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuLayoutItem &obj)
{
    argument.beginStructure();
    argument >> obj.id >> obj.properties;
    argument.beginArray();
    while (!argument.atEnd())
    {
        QDBusVariant dbusVariant;
        argument >> dbusVariant;
        const auto childArgument = dbusVariant.variant().value<QDBusArgument>();
        DBusMenuLayoutItem child;
        childArgument >> child;
        obj.children.append(child);
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

static void registerDBusMenuTypes()
{
    static bool registered = false;
    if (registered)
        return;

    qRegisterMetaType<DBusMenuLayoutItem>("DBusMenuLayoutItem");
    registered = true;
}

QVariantMap layoutItemToMap(const DBusMenuLayoutItem &item)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), item.id);
    map.insert(QStringLiteral("label"), item.properties.value(QStringLiteral("label")).toString());
    map.insert(QStringLiteral("enabled"), item.properties.value(QStringLiteral("enabled"), true));
    map.insert(QStringLiteral("visible"), item.properties.value(QStringLiteral("visible"), true));
    map.insert(QStringLiteral("separator"), item.properties.value(QStringLiteral("type")).toString() == QStringLiteral("separator"));
    map.insert(QStringLiteral("checkable"), !item.properties.value(QStringLiteral("toggle-type")).toString().isEmpty());
    map.insert(QStringLiteral("checked"), item.properties.value(QStringLiteral("toggle-state")).toInt() == 1);
    map.insert(QStringLiteral("iconName"), item.properties.value(QStringLiteral("icon-name")).toString());
    map.insert(QStringLiteral("childrenDisplay"), item.properties.value(QStringLiteral("children-display")).toString());

    QVariantList children;
    for (const DBusMenuLayoutItem &child : item.children)
        children.push_back(layoutItemToMap(child));
    map.insert(QStringLiteral("children"), children);
    return map;
}

QVariantList topLevelMenuItemsToList(const DBusMenuLayoutItem &root)
{
    QVariantList items;
    for (const DBusMenuLayoutItem &child : root.children)
        items.push_back(layoutItemToMap(child));
    return items;
}

constexpr auto kWatcherPath = "/StatusNotifierWatcher";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto kLocalWatcherService = "org.kde.StatusNotifierWatcher";
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
    if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral(":")))
        return {};

    return trimmed.section(QStringLiteral("."), -1);
}

bool shouldHideAnonymousTrayItem(const QString &service, const QString &title, const QString &iconName, const QString &menu)
{
    return service.trimmed().startsWith(QStringLiteral(":"))
        && title.trimmed().isEmpty()
        && iconName.trimmed() == QStringLiteral("application-x-executable")
        && menu.trimmed().isEmpty();
}

QString variantToString(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return {};

    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path().trimmed();

    if (value.canConvert<QString>())
        return value.toString().trimmed();

    return {};
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

QString lookupAppNameFromDesktopEntries(const QString &value)
{
    const QStringList dirs = desktopEntryDirs();
    if (dirs.isEmpty())
        return {};

    QStringList variants;
    addLookupVariants(&variants, value);
    if (variants.isEmpty())
        return {};

    for (const QString &dirPath : dirs)
    {
        QDirIterator iterator(dirPath,
                              QStringList { QStringLiteral("*.desktop") },
                              QDir::Files,
                              QDirIterator::Subdirectories);

        while (iterator.hasNext())
        {
            const QString filePath = iterator.next();
            QSettings desktopFile(filePath, QSettings::IniFormat);
            const QString appName = desktopFile.value(QStringLiteral("Desktop Entry/Name")).toString().trimmed();
            if (appName.isEmpty())
                continue;

            QStringList candidates;
            const QString appId = QFileInfo(filePath).completeBaseName();
            const QString startupWmClass = desktopFile.value(QStringLiteral("Desktop Entry/StartupWMClass")).toString().trimmed();
            const QString execField = desktopFile.value(QStringLiteral("Desktop Entry/Exec")).toString().trimmed();
            addUniqueCaseInsensitive(&candidates, appId);
            addUniqueCaseInsensitive(&candidates, startupWmClass);
            addUniqueCaseInsensitive(&candidates, shortCommandFromExec(execField));
            addUniqueCaseInsensitive(&candidates, QFileInfo(filePath).fileName());

            for (const QString &candidate : std::as_const(candidates))
            {
                const QString candidateKey = normalizedLookupKey(candidate);
                if (candidateKey.isEmpty())
                    continue;

                for (const QString &variant : std::as_const(variants))
                {
                    if (normalizedLookupKey(variant) == candidateKey)
                        return appName;
                }
            }
        }
    }

    return {};
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
    case MenuRole:
        return entry.menu;
    case ItemIsMenuRole:
        return entry.itemIsMenu;
    default:
        return {};
    }
}

QHash<int, QByteArray> SystemTrayController::roleNames() const
{
    return {
        {IdRole, "itemId"},
        {TitleRole, "title"},
        {IconNameRole, "iconName"},
        {IconSourceRole, "iconSource"},
        {StatusRole, "status"},
        {ServiceRole, "service"},
        {ObjectPathRole, "objectPath"},
        {MenuRole, "menu"},
        {ItemIsMenuRole, "itemIsMenu"},
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

QStringList SystemTrayController::registeredStatusNotifierItems() const
{
    return m_registeredItemIds;
}

bool SystemTrayController::isStatusNotifierHostRegistered() const
{
    return m_hostRegistered;
}

uint SystemTrayController::protocolVersion() const
{
    return 1;
}

void SystemTrayController::activate(int index)
{
    requestItemMethod(index, QStringLiteral("Activate"));
}

void SystemTrayController::secondaryActivate(int index)
{
    requestItemMethod(index, QStringLiteral("SecondaryActivate"));
}

void SystemTrayController::debugTrayItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return;

    const TrayItemEntry &entry = m_items.at(index);
    const auto safe = [](const QString &value) -> QString {
        return value.isEmpty() ? QStringLiteral("<empty>") : value;
    };

    qInfo().noquote() << QStringLiteral("System tray item %1\n  itemId: %2\n  title: %3\n  service: %4\n  objectPath: %5\n  status: %6\n  iconName: %7\n  iconSource: %8\n  menu: %9\n  itemIsMenu: %10")
                             .arg(QString::number(index),
                                  safe(entry.id),
                                  safe(entry.title),
                                  safe(entry.service),
                                  safe(entry.objectPath),
                                  safe(entry.status),
                                  safe(entry.iconName),
                                  safe(entry.iconSource),
                                  safe(entry.menu),
                                  QString::number(entry.itemIsMenu));
}

void SystemTrayController::contextMenu(int index, qreal x, qreal y)
{
    requestItemMethod(index, QStringLiteral("ContextMenu"), qRound(x), qRound(y));
}

void SystemTrayController::RegisterStatusNotifierItem(const QString &service)
{
    const QString trimmedService = service.trimmed();
    const QString senderService = message().service();
    const QString itemId = trimmedService.startsWith(QLatin1Char('/')) && !senderService.trimmed().isEmpty()
                              ? senderService.trimmed() + trimmedService
                              : trimmedService;
    if (itemId.isEmpty())
        return;

    if (!m_registeredItemIds.contains(itemId))
    {
        m_registeredItemIds.push_back(itemId);
        Q_EMIT registeredStatusNotifierItemsChanged();
        Q_EMIT StatusNotifierItemRegistered(itemId);
    }

    QTimer::singleShot(0, this, &SystemTrayController::refresh);
}

void SystemTrayController::RegisterStatusNotifierHost(const QString &service)
{
    Q_UNUSED(service)

    if (m_hostRegistered)
        return;

    m_hostRegistered = true;
    Q_EMIT isStatusNotifierHostRegisteredChanged();
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
    QStringList validIds;
    validIds.reserve(itemIds.size());

    for (const QString &itemId : itemIds)
    {
        const TrayItemEntry entry = buildItem(itemId);
        if (entry.service.isEmpty() || entry.objectPath.isEmpty())
        {
            qWarning() << "Skipping tray item" << itemId << "because service/object path could not be resolved";
            continue;
        }

        if (shouldHideAnonymousTrayItem(entry.service, entry.title, entry.iconName, entry.menu))
            continue;

        rebuilt.push_back(entry);
        validIds.push_back(itemId);
    }

    if (m_ownsWatcher && validIds != m_registeredItemIds)
    {
        const QStringList removedIds = m_registeredItemIds;
        for (const QString &itemId : removedIds)
        {
            if (!validIds.contains(itemId))
                Q_EMIT StatusNotifierItemUnregistered(itemId);
        }

        m_registeredItemIds = validIds;
        Q_EMIT registeredStatusNotifierItemsChanged();
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

    if (m_ownsWatcher && service == QString::fromLatin1(kLocalWatcherService))
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

    if (shouldHideAnonymousTrayItem(entry.service, entry.title, entry.iconName, entry.menu))
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

    if (m_ownsWatcher)
    {
        const int idIndex = m_registeredItemIds.indexOf(itemId);
        if (idIndex >= 0)
        {
            m_registeredItemIds.removeAt(idIndex);
            Q_EMIT registeredStatusNotifierItemsChanged();
        }
    }
}

bool SystemTrayController::parseItemId(const QString &itemId, QString *service, QString *objectPath, const QString &senderService)
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
    {
        const QString sender = senderService.trimmed();
        if (sender.isEmpty())
            return false;

        *service = sender;
        *objectPath = trimmed;
        return true;
    }

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
    if (m_watcherService == QString::fromLatin1(kLocalWatcherService))
        return QStringLiteral("org.kde.StatusNotifierWatcher");

    if (m_watcherService == QStringLiteral("org.freedesktop.StatusNotifierWatcher"))
        return QStringLiteral("org.freedesktop.StatusNotifierWatcher");

    return QStringLiteral("org.kde.StatusNotifierWatcher");
}

bool SystemTrayController::ensureLocalWatcher()
{
    if (m_ownsWatcher && m_watcherService == QString::fromLatin1(kLocalWatcherService))
        return true;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kLocalWatcherService)))
    {
        qWarning() << "Could not register local watcher service" << kLocalWatcherService;
        return false;
    }

    const bool objectRegistered = bus.registerObject(QString::fromLatin1(kWatcherPath),
                                                     this,
                                                     QDBusConnection::ExportAllSlots
                                                         | QDBusConnection::ExportAllSignals
                                                         | QDBusConnection::ExportAllProperties);
    if (!objectRegistered)
    {
        qWarning() << "Could not register local watcher object at" << kWatcherPath;
        bus.unregisterService(QString::fromLatin1(kLocalWatcherService));
        return false;
    }

    m_ownsWatcher = true;
    m_watcherService = QString::fromLatin1(kLocalWatcherService);
    m_watcherSignalsConnected = false;
    m_hostRegistered = false;
    return true;
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

    if (resolvedService.isEmpty())
    {
        if (!ensureLocalWatcher())
        {
            setAvailable(false);
            m_watcherService.clear();
            return false;
        }

        setAvailable(true);
        return true;
    }

    if (resolvedService != m_watcherService)
    {
        m_watcherService = resolvedService;
        m_watcherSignalsConnected = false;
        m_hostRegistered = false;
        m_ownsWatcher = false;
    }

    const bool hasWatcher = !m_watcherService.isEmpty();
    setAvailable(hasWatcher);
    return hasWatcher;
}

bool SystemTrayController::connectWatcherSignals()
{
    if (m_ownsWatcher)
        return true;

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

    if (m_ownsWatcher)
    {
        RegisterStatusNotifierHost(hostService);
        return;
    }

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
    if (m_watcherService.isEmpty())
        return {};

    if (m_ownsWatcher && m_watcherService == QString::fromLatin1(kLocalWatcherService))
    {
        QVector<QString> ids;
        ids.reserve(m_registeredItemIds.size());
        for (const QString &itemId : m_registeredItemIds)
        {
            const QString trimmed = itemId.trimmed();
            if (!trimmed.isEmpty())
                ids.push_back(trimmed);
        }
        return ids;
    }

    QVector<QString> ids;

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
    const QString desktopEntryProperty = properties.value(QStringLiteral("DesktopEntry")).toString().trimmed();

    if (entry.title.isEmpty() || entry.title.startsWith(QLatin1Char(':')))
    {
        const QStringList titleCandidates {
            desktopEntryProperty,
            itemIdProperty,
            entry.service,
            shortServiceName(entry.service)
        };

        for (const QString &candidate : titleCandidates)
        {
            const QString appName = lookupAppNameFromDesktopEntries(candidate);
            if (!appName.isEmpty())
            {
                entry.title = appName;
                break;
            }
        }
    }

    if (entry.title.isEmpty() || entry.title.startsWith(QLatin1Char(':')))
        entry.title = itemIdProperty;
    if (entry.title.isEmpty() || entry.title.startsWith(QLatin1Char(':')))
        entry.title = shortServiceName(entry.service);
    if (entry.title.startsWith(QLatin1Char(':')))
        entry.title.clear();
    if (entry.title.isEmpty() && !entry.service.trimmed().startsWith(QLatin1Char(':')))
        entry.title = entry.service;

    entry.iconName = variantToString(properties.value(QStringLiteral("IconName")));
    entry.attentionIconName = variantToString(properties.value(QStringLiteral("AttentionIconName")));
    entry.menu = variantToString(properties.value(QStringLiteral("Menu")));
    entry.itemIsMenu = properties.value(QStringLiteral("ItemIsMenu")).toBool();

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

void SystemTrayController::requestItemMethod(int index, const QString &method, int x, int y)
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
        {
            continue;
        }

        itemIface.call(method, x, y);
        break;
    }
}

QVariantList SystemTrayController::trayMenuItems(int index) const
{
    QVariantList items;
    if (index < 0 || index >= m_items.size())
        return items;

    const TrayItemEntry &entry = m_items.at(index);
    if (entry.service.isEmpty() || entry.objectPath.isEmpty())
        return items;

    registerDBusMenuTypes();

    if (!entry.menu.trimmed().isEmpty())
    {
        QDBusInterface menuIface(entry.service,
                                 entry.menu,
                                 QStringLiteral("com.canonical.dbusmenu"),
                                 QDBusConnection::sessionBus());
        if (menuIface.isValid())
        {
            const QDBusMessage reply = menuIface.call(QStringLiteral("GetLayout"), 0, 1, QStringList());
            if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() >= 2)
            {
                const QVariant layoutVariant = reply.arguments().at(1);
                DBusMenuLayoutItem root;
                if (layoutVariant.canConvert<QDBusArgument>())
                {
                    const QDBusArgument layoutArgument = layoutVariant.value<QDBusArgument>();
                    layoutArgument >> root;
                }
                else
                {
                    root = layoutVariant.value<DBusMenuLayoutItem>();
                }
                items = topLevelMenuItemsToList(root);
            }
        }
    }

    if (!items.isEmpty())
        return items;

    const QString activateText = entry.itemIsMenu ? QStringLiteral("Open") : QStringLiteral("Activate");
    items.push_back(QVariantMap {
        {QStringLiteral("id"), -1},
        {QStringLiteral("label"), activateText},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("visible"), true},
        {QStringLiteral("separator"), false},
        {QStringLiteral("checkable"), false},
        {QStringLiteral("checked"), false},
        {QStringLiteral("iconName"), entry.itemIsMenu ? QStringLiteral("view-more-symbolic") : QStringLiteral("document-open")},
        {QStringLiteral("children"), QVariantList {}},
    });
    items.push_back(QVariantMap {
        {QStringLiteral("id"), -2},
        {QStringLiteral("label"), QStringLiteral("Secondary Activate")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("visible"), true},
        {QStringLiteral("separator"), false},
        {QStringLiteral("checkable"), false},
        {QStringLiteral("checked"), false},
        {QStringLiteral("iconName"), QStringLiteral("media-playback-pause")},
        {QStringLiteral("children"), QVariantList {}},
    });
    items.push_back(QVariantMap {
        {QStringLiteral("id"), -3},
        {QStringLiteral("label"), QStringLiteral("Show Menu")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("visible"), true},
        {QStringLiteral("separator"), false},
        {QStringLiteral("checkable"), false},
        {QStringLiteral("checked"), false},
        {QStringLiteral("iconName"), QStringLiteral("open-menu-symbolic")},
        {QStringLiteral("children"), QVariantList {}},
    });

    return items;
}

void SystemTrayController::triggerTrayMenuItem(int index, int itemId)
{
    if (index < 0 || index >= m_items.size())
        return;

    const TrayItemEntry &entry = m_items.at(index);
    if (entry.service.isEmpty() || entry.objectPath.isEmpty())
        return;

    if (itemId == -1)
    {
        activate(index);
        return;
    }

    if (itemId == -2)
    {
        secondaryActivate(index);
        return;
    }

    if (itemId == -3 || entry.menu.trimmed().isEmpty())
    {
        contextMenu(index);
        return;
    }

    registerDBusMenuTypes();
    QDBusInterface menuIface(entry.service,
                             entry.menu,
                             QStringLiteral("com.canonical.dbusmenu"),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid())
        return;

    QDBusMessage eventCall = QDBusMessage::createMethodCall(entry.service,
                                                            entry.menu,
                                                            QStringLiteral("com.canonical.dbusmenu"),
                                                            QStringLiteral("Event"));
    eventCall << itemId << QStringLiteral("clicked") << QString() << 0u;
    QDBusConnection::sessionBus().call(eventCall);
}

void SystemTrayController::setAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    Q_EMIT availableChanged(m_available);
}
