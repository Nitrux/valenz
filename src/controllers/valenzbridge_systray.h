#pragma once

#include <QAbstractListModel>
#include <QDBusContext>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantMap>
#include <QVariantList>

class QDBusServiceWatcher;
class QTimer;

class SystemTrayController : public QAbstractListModel, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredStatusNotifierItems NOTIFY registeredStatusNotifierItemsChanged FINAL)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isStatusNotifierHostRegistered NOTIFY isStatusNotifierHostRegisteredChanged FINAL)
    Q_PROPERTY(uint ProtocolVersion READ protocolVersion CONSTANT FINAL)

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        IconNameRole,
        IconSourceRole,
        StatusRole,
        ServiceRole,
        ObjectPathRole,
        MenuRole,
        ItemIsMenuRole,
    };

    explicit SystemTrayController(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool available() const;

    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void secondaryActivate(int index);
    Q_INVOKABLE void contextMenu(int index, qreal x = 0, qreal y = 0);
    Q_INVOKABLE QVariantList trayMenuItems(int index) const;
    Q_INVOKABLE void debugTrayItem(int index);
    Q_INVOKABLE void triggerTrayMenuItem(int index, int itemId);
    Q_INVOKABLE void refresh();

public Q_SLOTS:
    void RegisterStatusNotifierItem(const QString &service);
    void RegisterStatusNotifierHost(const QString &service);

Q_SIGNALS:
    void countChanged();
    void availableChanged(bool available);
    void registeredStatusNotifierItemsChanged();
    void isStatusNotifierHostRegisteredChanged();
    void StatusNotifierItemRegistered(const QString &itemId);
    void StatusNotifierItemUnregistered(const QString &itemId);

private Q_SLOTS:
    void onWatcherServiceOwnerChanged(const QString &service, const QString &oldOwner, const QString &newOwner);
    void onStatusNotifierItemRegistered(const QString &itemId);
    void onStatusNotifierItemUnregistered(const QString &itemId);

private:
    struct TrayItemEntry
    {
        QString id;
        QString service;
        QString objectPath;
        QString title;
        QString iconName;
        QString iconSource;
        QString attentionIconName;
        QString menu;
        QString status;
        bool itemIsMenu = false;
    };

    static bool parseItemId(const QString &itemId, QString *service, QString *objectPath, const QString &senderService = {});
    static QString effectiveIconName(const TrayItemEntry &entry);
    static QString effectiveIconSource(const TrayItemEntry &entry);

    QString watcherService() const;
    QString watcherInterface() const;
    bool ensureWatcher();
    bool ensureLocalWatcher();
    bool connectWatcherSignals();
    void registerAsHost();

    QVector<QString> fetchRegisteredItemIds() const;
    TrayItemEntry buildItem(const QString &itemId) const;
    QVariantMap fetchItemProperties(const QString &service, const QString &objectPath, const QString &itemInterface) const;

    int indexOfItemId(const QString &itemId) const;
    void replaceAllItems(const QVector<TrayItemEntry> &items);
    void requestItemMethod(int index, const QString &method, int x = 0, int y = 0);
    void setAvailable(bool available);
    QStringList registeredStatusNotifierItems() const;
    bool isStatusNotifierHostRegistered() const;
    uint protocolVersion() const;

    QVector<TrayItemEntry> m_items;
    QDBusServiceWatcher *m_watcherServiceWatcher = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QString m_watcherService;
    QStringList m_registeredItemIds;
    bool m_available = false;
    bool m_watcherSignalsConnected = false;
    bool m_hostRegistered = false;
    bool m_ownsWatcher = false;
};
