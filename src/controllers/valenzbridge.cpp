#include "valenzbridge.h"
#include "valenzbridge_p.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QTemporaryDir>

ValenzBridge::ValenzBridge(QObject *parent)
    : QObject(parent)
{
    initializeConfig();
    setAgendaInstalled(isAgendaInstalled());

    m_weatherNetwork = new QNetworkAccessManager(this);
    connect(m_weatherNetwork, &QNetworkAccessManager::finished, this, &ValenzBridge::onWeatherReplyFinished);

    m_artworkNetwork = new QNetworkAccessManager(this);
    m_artworkCacheDir = new QTemporaryDir(QDir::tempPath() + QStringLiteral("/valenz-artwork-XXXXXX"));
    m_artworkTimeoutTimer = new QTimer(this);
    m_artworkTimeoutTimer->setSingleShot(true);
    connect(m_artworkTimeoutTimer, &QTimer::timeout, this, [this] {
        if (m_artworkReply)
            m_artworkReply->abort();
    });

    m_weatherRefreshTimer = new QTimer(this);
    m_weatherRefreshTimer->setTimerType(Qt::CoarseTimer);
    connect(m_weatherRefreshTimer, &QTimer::timeout, this, &ValenzBridge::refreshWeather);
    updateWeatherRefreshTimerInterval();
    refreshWorkspaceState();
    refreshFocusedWindowState();
    connectHyprlandEventSocket();
    connectMprisSignalObservers();

    m_mprisRefreshTimer = new QTimer(this);
    m_mprisRefreshTimer->setInterval(kMprisRefreshIntervalMs);
    m_mprisRefreshTimer->setTimerType(Qt::CoarseTimer);
    connect(m_mprisRefreshTimer, &QTimer::timeout, this, &ValenzBridge::refreshMprisState);

    m_mprisPlaybackTimer = new QTimer(this);
    m_mprisPlaybackTimer->setInterval(kMprisPlaybackTickMs);
    m_mprisPlaybackTimer->setTimerType(Qt::CoarseTimer);
    connect(m_mprisPlaybackTimer, &QTimer::timeout, this, &ValenzBridge::updateMprisTimestampFromTicker);

    refreshMprisState();
    updateMprisRefreshTimer();
    QTimer::singleShot(0, this, &ValenzBridge::refreshWeather);
    initializeControlCenterRuntime();
    initializeConfigWatcher();
}

ValenzBridge::~ValenzBridge()
{
    cancelMediaArtworkRequest();
    delete m_artworkCacheDir;
}

void ValenzBridge::toggleVicinae()
{
    QProcess::startDetached(QStringLiteral("vicinae"), {QStringLiteral("toggle")});
}
