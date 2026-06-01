#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QVariantMap>

class QDBusServiceWatcher;
class QTimer;

class SystemTrayController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)

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
    };

    explicit SystemTrayController(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool available() const;

    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void secondaryActivate(int index);
    Q_INVOKABLE void contextMenu(int index);
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void availableChanged(bool available);

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
        QString status;
    };

    static bool parseItemId(const QString &itemId, QString *service, QString *objectPath);
    static QString effectiveIconName(const TrayItemEntry &entry);
    static QString effectiveIconSource(const TrayItemEntry &entry);

    QString watcherService() const;
    QString watcherInterface() const;
    bool ensureWatcher();
    bool connectWatcherSignals();
    void registerAsHost();

    QVector<QString> fetchRegisteredItemIds() const;
    TrayItemEntry buildItem(const QString &itemId) const;
    QVariantMap fetchItemProperties(const QString &service, const QString &objectPath, const QString &itemInterface) const;

    int indexOfItemId(const QString &itemId) const;
    void replaceAllItems(const QVector<TrayItemEntry> &items);
    void requestItemMethod(int index, const QString &method);
    void setAvailable(bool available);

    QVector<TrayItemEntry> m_items;
    QDBusServiceWatcher *m_watcherServiceWatcher = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QString m_watcherService;
    bool m_available = false;
    bool m_watcherSignalsConnected = false;
    bool m_hostRegistered = false;
};
