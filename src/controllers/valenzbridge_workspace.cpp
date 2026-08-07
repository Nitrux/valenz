#include "valenzbridge.h"
#include "valenzbridge_p.h"

namespace
{
constexpr qsizetype kMaximumHyprlandEventLineBytes = 16 * 1024;
constexpr qint64 kHyprlandReadChunkBytes = 4096;
}

void ValenzBridge::goToPreviousWorkspace()
{
    if (!dispatchWorkspaceFocus(QStringLiteral("-1")))
        return;

    scheduleWorkspaceStateRefresh(50);
}

void ValenzBridge::goToNextWorkspace()
{
    if (!dispatchWorkspaceFocus(QStringLiteral("+1")))
        return;

    scheduleWorkspaceStateRefresh(50);
}

void ValenzBridge::scheduleWorkspaceStateRefresh(int delayMs)
{
    if (delayMs <= 0)
    {
        refreshWorkspaceState();
        return;
    }

    if (!m_workspaceRefreshTimer)
    {
        m_workspaceRefreshTimer = new QTimer(this);
        m_workspaceRefreshTimer->setSingleShot(true);
        m_workspaceRefreshTimer->setTimerType(Qt::CoarseTimer);
        connect(m_workspaceRefreshTimer, &QTimer::timeout, this, [this]()
        {
            refreshWorkspaceState();
        });
    }

    m_workspaceRefreshTimer->start(delayMs);
}

void ValenzBridge::scheduleFocusedWindowStateRefresh(int delayMs)
{
    if (delayMs <= 0)
    {
        refreshFocusedWindowState();
        return;
    }

    if (!m_focusedWindowRefreshTimer)
    {
        m_focusedWindowRefreshTimer = new QTimer(this);
        m_focusedWindowRefreshTimer->setSingleShot(true);
        m_focusedWindowRefreshTimer->setTimerType(Qt::CoarseTimer);
        connect(m_focusedWindowRefreshTimer, &QTimer::timeout, this, [this]()
        {
            refreshFocusedWindowState();
        });
    }

    m_focusedWindowRefreshTimer->start(delayMs);
}

bool ValenzBridge::refreshWorkspaceState()
{
    QJsonValue activeWorkspace;
    QJsonValue workspaces;

    if (!runHyprctlJson(QStringList { QStringLiteral("-j"), QStringLiteral("activeworkspace") }, &activeWorkspace))
    {
        return false;
    }

    if (!runHyprctlJson(QStringList { QStringLiteral("-j"), QStringLiteral("workspaces") }, &workspaces))
    {
        return false;
    }

    const int currentWorkspace = hyprlandCurrentWorkspace(activeWorkspace);
    const int workspaceCount = hyprlandWorkspaceCount(workspaces);

    if (currentWorkspace < 1 || workspaceCount < 1)
    {
        return false;
    }

    setWorkspaceCount(workspaceCount);
    setCurrentWorkspace(currentWorkspace);
    refreshWindowList();
    return true;
}

bool ValenzBridge::refreshWindowList()
{
    QJsonValue clients;
    if (!runHyprctlJson(QStringList { QStringLiteral("-j"), QStringLiteral("clients") }, &clients))
        return false;

    QJsonValue activeWindow;
    const bool hasActiveWindow = runHyprctlJson(QStringList { QStringLiteral("-j"), QStringLiteral("activewindow") }, &activeWindow);
    const QString activeAddress = hasActiveWindow && activeWindow.isObject()
                                      ? activeWindow.toObject().value(QStringLiteral("address")).toString()
                                      : QString();

    QVariantList windows;
    if (clients.isArray())
    {
        for (const QJsonValue &clientValue : clients.toArray())
        {
            if (!clientValue.isObject())
                continue;

            const QJsonObject client = clientValue.toObject();
            const int workspace = client.value(QStringLiteral("workspace")).toObject().value(QStringLiteral("id")).toInt(-1);
            if (workspace != m_currentWorkspace)
                continue;

            const QString address = client.value(QStringLiteral("address")).toString().trimmed();
            if (address.isEmpty())
                continue;

            QString title = client.value(QStringLiteral("title")).toString().trimmed();
            if (title.isEmpty())
                title = client.value(QStringLiteral("initialTitle")).toString().trimmed();

            QString resolvedIconName = QStringLiteral("application-x-executable");
            QStringList iconCandidates;
            addWindowIconCandidates(&iconCandidates, client.value(QStringLiteral("class")).toString());
            addWindowIconCandidates(&iconCandidates, client.value(QStringLiteral("initialClass")).toString());
            const qint64 pid = client.value(QStringLiteral("pid")).toVariant().toLongLong();
            addWindowIconCandidates(&iconCandidates, processNameFromPid(pid));

            for (const QString &candidate : std::as_const(iconCandidates))
            {
                if (isUsableIconSource(candidate))
                {
                    resolvedIconName = candidate;
                    break;
                }
            }

            if (resolvedIconName == QLatin1String("application-x-executable"))
            {
                for (const QString &candidate : std::as_const(iconCandidates))
                {
                    const QString mappedIcon = lookupIconFromDesktopEntries(candidate);
                    if (mappedIcon.isEmpty())
                        continue;

                    if (isUsableIconSource(mappedIcon))
                    {
                        resolvedIconName = mappedIcon;
                        break;
                    }

                    if (resolvedIconName == QLatin1String("application-x-executable"))
                        resolvedIconName = mappedIcon;
                }
            }

            QVariantMap window;
            window.insert(QStringLiteral("address"), address);
            window.insert(QStringLiteral("title"), title);
            window.insert(QStringLiteral("iconName"), resolvedIconName);
            window.insert(QStringLiteral("focused"), address == activeAddress);
            windows.append(window);
        }
    }

    if (m_windowList != windows)
    {
        m_windowList = windows;
        Q_EMIT windowListChanged();
    }

    return true;
}

void ValenzBridge::focusWindow(const QString &address)
{
    const QString normalizedAddress = address.trimmed();
    if (normalizedAddress.isEmpty())
        return;

    runHyprctlDispatch(QStringList { QStringLiteral("dispatch"),
                                     QStringLiteral("focuswindow"),
                                     QStringLiteral("address:") + normalizedAddress });
}

bool ValenzBridge::refreshFocusedWindowState()
{
    QJsonValue activeWindow;

    if (!runHyprctlJson(QStringList { QStringLiteral("-j"), QStringLiteral("activewindow") }, &activeWindow))
    {
        setFocusedWindowTitle(QString());
        setFocusedWindowIconName(QStringLiteral("application-x-executable"));
        setFocusedWindowFullscreenInternal(kFullscreenModeNone);
        setFocusedWindowFullscreenClient(kFullscreenModeNone);
        refreshWindowList();
        return false;
    }

    if (!activeWindow.isObject())
    {
        setFocusedWindowTitle(QString());
        setFocusedWindowIconName(QStringLiteral("application-x-executable"));
        setFocusedWindowFullscreenInternal(kFullscreenModeNone);
        setFocusedWindowFullscreenClient(kFullscreenModeNone);
        refreshWindowList();
        return false;
    }

    const QJsonObject windowObject = activeWindow.toObject();

    const auto parseFullscreenMode = [](const QJsonValue &value) -> int
    {
        if (value.isBool())
            return value.toBool() ? kFullscreenModeFullscreen : kFullscreenModeNone;

        return qBound(kFullscreenModeNone, value.toInt(), kFullscreenModeMax);
    };

    const int fullscreenInternal = parseFullscreenMode(windowObject.value(QStringLiteral("fullscreen")));
    const int fullscreenClient = parseFullscreenMode(windowObject.value(QStringLiteral("fullscreenClient")));

    QString title = windowObject.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty())
        title = windowObject.value(QStringLiteral("initialTitle")).toString().trimmed();

    QString resolvedIconName = QStringLiteral("application-x-executable");

    QStringList iconCandidates;
    addWindowIconCandidates(&iconCandidates, windowObject.value(QStringLiteral("class")).toString());
    addWindowIconCandidates(&iconCandidates, windowObject.value(QStringLiteral("initialClass")).toString());

    const qint64 pid = windowObject.value(QStringLiteral("pid")).toVariant().toLongLong();
    addWindowIconCandidates(&iconCandidates, processNameFromPid(pid));

    for (const QString &candidate : std::as_const(iconCandidates))
    {
        if (!isUsableIconSource(candidate))
            continue;

        resolvedIconName = candidate;
        break;
    }

    if (resolvedIconName == QLatin1String("application-x-executable"))
    {
        for (const QString &candidate : std::as_const(iconCandidates))
        {
            const QString mappedIcon = lookupIconFromDesktopEntries(candidate);
            if (mappedIcon.isEmpty())
                continue;

            if (isUsableIconSource(mappedIcon))
            {
                resolvedIconName = mappedIcon;
                break;
            }

            if (resolvedIconName == QLatin1String("application-x-executable"))
                resolvedIconName = mappedIcon;
        }
    }

    setFocusedWindowTitle(title);
    setFocusedWindowIconName(resolvedIconName);
    setFocusedWindowFullscreenInternal(fullscreenInternal);
    setFocusedWindowFullscreenClient(fullscreenClient);
    refreshWindowList();
    return true;
}

void ValenzBridge::connectHyprlandEventSocket()
{
    const QString socketPath = hyprlandEventSocketPath();
    if (socketPath.isEmpty())
        return;

    if (!m_hyprlandEventSocket)
    {
        m_hyprlandEventSocket = new QLocalSocket(this);

        connect(m_hyprlandEventSocket, &QLocalSocket::readyRead, this, &ValenzBridge::handleHyprlandEventData);

        connect(m_hyprlandEventSocket, &QLocalSocket::disconnected, this,
                [this]()
        {
            m_hyprlandEventBuffer.clear();
            m_discardOversizedHyprlandEvent = false;
            scheduleHyprlandEventSocketReconnect();
        });

        connect(m_hyprlandEventSocket, &QLocalSocket::errorOccurred, this,
                [this](QLocalSocket::LocalSocketError)
        {
            scheduleHyprlandEventSocketReconnect();
        });
    }

    if (m_hyprlandEventSocket->state() == QLocalSocket::ConnectedState
        || m_hyprlandEventSocket->state() == QLocalSocket::ConnectingState)
    {
        return;
    }

    m_hyprlandEventBuffer.clear();
    m_discardOversizedHyprlandEvent = false;
    m_hyprlandEventSocket->abort();
    m_hyprlandEventSocket->connectToServer(socketPath, QIODevice::ReadOnly);
}

void ValenzBridge::scheduleHyprlandEventSocketReconnect()
{
    QTimer::singleShot(2000, this,
                       [this]()
    {
        connectHyprlandEventSocket();
    });
}

void ValenzBridge::handleHyprlandEventData()
{
    if (!m_hyprlandEventSocket)
        return;

    while (m_hyprlandEventSocket->bytesAvailable() > 0)
    {
        QByteArray chunk = m_hyprlandEventSocket->read(kHyprlandReadChunkBytes);
        if (chunk.isEmpty())
            break;

        if (m_discardOversizedHyprlandEvent)
        {
            const qsizetype newlineIndex = chunk.indexOf('\n');
            if (newlineIndex < 0)
                continue;

            chunk.remove(0, newlineIndex + 1);
            m_discardOversizedHyprlandEvent = false;
        }

        m_hyprlandEventBuffer += chunk;
        qsizetype newlineIndex = m_hyprlandEventBuffer.indexOf('\n');
        while (newlineIndex >= 0)
        {
            if (newlineIndex <= kMaximumHyprlandEventLineBytes)
            {
                const QByteArray lineBytes = m_hyprlandEventBuffer.left(newlineIndex).trimmed();
                if (!lineBytes.isEmpty())
                    handleHyprlandEventLine(QString::fromUtf8(lineBytes));
            }

            m_hyprlandEventBuffer.remove(0, newlineIndex + 1);
            newlineIndex = m_hyprlandEventBuffer.indexOf('\n');
        }

        if (m_hyprlandEventBuffer.size() > kMaximumHyprlandEventLineBytes)
        {
            m_hyprlandEventBuffer.clear();
            m_discardOversizedHyprlandEvent = true;
        }
    }
}

void ValenzBridge::handleHyprlandEventLine(const QString &line)
{
    const int separatorIndex = line.indexOf(QStringLiteral(">>"));
    if (separatorIndex <= 0)
        return;

    const QString eventName = line.left(separatorIndex).trimmed();

    if (isWorkspaceRelatedHyprlandEvent(eventName))
        scheduleWorkspaceStateRefresh(50);

    if (isFocusedWindowRelatedHyprlandEvent(eventName))
        scheduleFocusedWindowStateRefresh(50);
}

