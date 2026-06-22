#include "valenzbridge.h"
#include "valenzbridge_p.h"

void ValenzBridge::goToPreviousWorkspace()
{

    if (!dispatchWorkspaceFocus(QStringLiteral("-1")))
    {
        return;
    }
    refreshWorkspaceState();

}

void ValenzBridge::goToNextWorkspace()
{

    if (!dispatchWorkspaceFocus(QStringLiteral("+1")))
    {
        return;
    }
    refreshWorkspaceState();

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
    return true;
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
        return false;
    }

    if (!activeWindow.isObject())
    {
        setFocusedWindowTitle(QString());
        setFocusedWindowIconName(QStringLiteral("application-x-executable"));
        setFocusedWindowFullscreenInternal(kFullscreenModeNone);
        setFocusedWindowFullscreenClient(kFullscreenModeNone);
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

    m_hyprlandEventBuffer += m_hyprlandEventSocket->readAll();

    int newlineIndex = m_hyprlandEventBuffer.indexOf('\n');
    while (newlineIndex >= 0)
    {
        const QByteArray lineBytes = m_hyprlandEventBuffer.left(newlineIndex).trimmed();
        m_hyprlandEventBuffer.remove(0, newlineIndex + 1);

        if (!lineBytes.isEmpty())
            handleHyprlandEventLine(QString::fromUtf8(lineBytes));

        newlineIndex = m_hyprlandEventBuffer.indexOf('\n');
    }
}

void ValenzBridge::handleHyprlandEventLine(const QString &line)
{
    const int separatorIndex = line.indexOf(QStringLiteral(">>"));
    if (separatorIndex <= 0)
        return;

    const QString eventName = line.left(separatorIndex).trimmed();

    if (isWorkspaceRelatedHyprlandEvent(eventName))
        refreshWorkspaceState();

    if (isFocusedWindowRelatedHyprlandEvent(eventName))
        refreshFocusedWindowState();
}

