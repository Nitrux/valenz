#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class NotificationsController : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool dndEnabled READ dndEnabled WRITE setDndEnabled NOTIFY dndEnabledChanged FINAL)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
    Q_PROPERTY(QVariantList groupedNotifications READ groupedNotifications NOTIFY notificationsChanged FINAL)

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        SourceNameRole,
        MessageTextRole,
        TimestampTextRole,
        IconNameRole,
        UrgencyLevelRole,
        ActionTextRole,
        ActionKeyRole,
    };

    explicit NotificationsController(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool dndEnabled() const;
    bool available() const;
    QVariantList groupedNotifications() const;

    Q_INVOKABLE void clearAllNotifications();
    Q_INVOKABLE void dismiss(int index);
    Q_INVOKABLE void dismissById(uint id);
    Q_INVOKABLE void dismissGroup(const QString &sourceName);
    Q_INVOKABLE void invokeAction(int index);
    Q_INVOKABLE void invokeActionById(uint id);
    Q_INVOKABLE void refreshTimestamps();

public Q_SLOTS:
    void setDndEnabled(bool enabled);
    void toggleDnd();

    uint Notify(const QString &appName,
                uint replacesId,
                const QString &appIcon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int timeout);
    void CloseNotification(uint id);
    QStringList GetCapabilities() const;
    QString GetServerInformation(QString &vendor, QString &version, QString &specVersion) const;

Q_SIGNALS:
    void countChanged(int count);
    void dndEnabledChanged(bool enabled);
    void availableChanged(bool available);
    void notificationsChanged();
    void transientNotification(uint id, const QString &sourceName, const QString &messageText, const QString &timestampText, const QString &iconName, int urgencyLevel, const QString &actionText, const QString &actionKey);

    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    struct NotificationEntry
    {
        uint id = 0;
        QString sourceName;
        QString messageText;
        QDateTime createdAt;
        QString iconName;
        int urgencyLevel = 0;
        QString actionText;
        QString actionKey;
    };

    int indexOfId(uint id) const;
    QString relativeTimestamp(const QDateTime &createdAt) const;
    static QString normalizedGroupKey(const QString &sourceName);
    static int parseUrgency(const QVariantMap &hints);
    static QString chooseActionText(const QStringList &actions);
    static QString chooseActionKey(const QStringList &actions);
    static QString normalizeIconName(const QString &iconName, const QString &appName, const QVariantMap &hints);
    QVariantMap notificationEntryToMap(const NotificationEntry &entry) const;

    void removeByIndex(int row, uint closeReason);
    void setAvailable(bool available);

    QVector<NotificationEntry> m_entries;
    uint m_nextId = 1;
    bool m_dndEnabled = false;
    bool m_available = false;
};
