#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QThreadPool>

namespace
{
constexpr int kControlCenterIdleRefreshIntervalMs = 3000;
constexpr int kControlCenterActiveRefreshIntervalMs = 1000;

bool startDetachedCommand(const QString &command)
{
    QStringList parts = QProcess::splitCommand(command.trimmed());
    if (parts.isEmpty())
        return false;

    const QString program = parts.takeFirst();
    return !program.isEmpty() && QProcess::startDetached(program, parts);
}

struct ControlCenterRuntimeSnapshot
{
    bool wirelessAvailable = false;
    bool wirelessEnabled = false;
    bool wirelessConnected = false;
    bool wiredConnected = false;
    QString networkState = QStringLiteral("offline");
    bool bluetoothAvailable = false;
    int bluetoothConnectedDeviceCount = 0;
    bool bluetoothEnabled = false;
    QString volumePercentage = QStringLiteral("0%");
    bool volumeMuted = false;
    QString microphoneVolumePercentage = QStringLiteral("0%");
    bool microphoneAvailable = false;
    bool brightnessAvailable = false;
    QString brightnessPercentage = QStringLiteral("0%");
    bool batteryAvailable = false;
    bool batteryOnAcPower = false;
    bool batteryCharging = false;
    QString batteryPercentage = QStringLiteral("0%");
    bool nightLightAvailable = false;
    bool nightLightEnabled = false;
    bool powerProfileCurrentValid = false;
    QString powerProfileCurrent;
    QStringList powerProfiles;
};

struct MicrophoneSnapshot
{
    QString volumePercentage = QStringLiteral("0%");
    bool available = false;
};

MicrophoneSnapshot collectMicrophoneSnapshot()
{
    MicrophoneSnapshot snapshot;
    QString inspect;
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList {QStringLiteral("inspect"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@")}, &inspect, 1000))
        return snapshot;

    QString volumeOutput;
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@")}, &volumeOutput, 1000))
        return snapshot;
    const QStringList volumeFields = volumeOutput.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (volumeFields.size() < 2)
        return snapshot;
    bool volumeOk = false;
    const double currentVolume = volumeFields.at(1).toDouble(&volumeOk);
    if (!volumeOk)
        return snapshot;
    snapshot.volumePercentage = QStringLiteral("%1%").arg(qBound(0, qRound(currentVolume * 100.0), 150));

    bool availabilityKnown = false;
    for (const QString &line : inspect.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        if (!line.contains(QStringLiteral("port.availability"), Qt::CaseInsensitive))
            continue;
        QString availability = line.section(QLatin1Char('='), 1).trimmed();
        availability.remove(QLatin1Char('"'));
        availability = availability.toLower();
        if (availability == QLatin1String("available"))
        {
            availabilityKnown = true;
            snapshot.available = true;
        }
        else if (availability == QLatin1String("not available") || availability == QLatin1String("unavailable"))
        {
            availabilityKnown = true;
            snapshot.available = false;
        }
        if (availabilityKnown)
            break;
    }
    if (availabilityKnown)
        return snapshot;

    static QElapsedTimer probeTimer;
    static bool probeAvailable = false;
    if (probeTimer.isValid() && probeTimer.elapsed() < 5000)
    {
        snapshot.available = probeAvailable;
        return snapshot;
    }
    probeTimer.restart();

    const double testVolume = currentVolume >= 1.49 ? 1.48 : currentVolume + 0.01;
    const bool changed = MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@"), QString::number(testVolume, 'f', 2)}, nullptr, 1000);
    QString afterOutput;
    const bool readBack = changed && MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@")}, &afterOutput, 1000);
    MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@"), QString::number(currentVolume, 'f', 2)}, nullptr, 1000);
    const QStringList afterFields = afterOutput.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    bool afterOk = false;
    const double afterVolume = afterFields.size() > 1 ? afterFields.at(1).toDouble(&afterOk) : 0.0;
    snapshot.available = readBack && afterOk && qAbs(afterVolume - testVolume) >= 0.005;
    probeAvailable = snapshot.available;
    return snapshot;
}

struct ControlCenterSystemResourcesSnapshot
{
    bool cpuValid = false;
    int cpuPercentage = 0;
    bool ramValid = false;
    int ramPercentage = 0;
    QString diskText;
    int diskPercentage = 0;
    bool diskValid = false;
};

void collectControlCenterNetworkState(ControlCenterRuntimeSnapshot *snapshot)
{
    if (!snapshot)
        return;

    snapshot->wirelessAvailable = MauiKitSystem::controlCenterWirelessAvailable();

    QString radioState;
    if (snapshot->wirelessAvailable
        && MauiKitSystem::runCommandText(QStringLiteral("nmcli"),
                                        QStringList { QStringLiteral("radio"), QStringLiteral("wifi") },
                                        &radioState))
    {
        snapshot->wirelessEnabled = radioState.trimmed().compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0;
    }

    QString deviceStatus;
    if (MauiKitSystem::runCommandText(QStringLiteral("nmcli"),
                                      QStringList { QStringLiteral("-t"),
                                                    QStringLiteral("-f"),
                                                    QStringLiteral("TYPE,STATE"),
                                                    QStringLiteral("device"),
                                                    QStringLiteral("status") },
                                      &deviceStatus))
    {
        const QStringList lines = deviceStatus.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            const QStringList fields = line.split(QLatin1Char(':'));
            if (fields.size() < 2)
                continue;

            const QString type = fields.at(0).trimmed().toLower();
            const QString state = fields.at(1).trimmed().toLower();
            if (!state.startsWith(QStringLiteral("connected")))
                continue;

            if (type == QLatin1String("ethernet"))
                snapshot->wiredConnected = true;
            else if (type == QLatin1String("wifi") || type == QLatin1String("wireless"))
                snapshot->wirelessConnected = true;
        }
    }

    if (snapshot->wirelessConnected)
        snapshot->wirelessEnabled = true;

    const QString iface = MauiKitSystem::defaultRouteInterface();
    if (!iface.isEmpty())
    {
        snapshot->networkState = MauiKitSystem::isWirelessInterface(iface)
                ? QStringLiteral("wireless")
                : QStringLiteral("wired");
    }
    else if (snapshot->wiredConnected)
    {
        snapshot->networkState = QStringLiteral("wired");
    }
    else if (snapshot->wirelessConnected)
    {
        snapshot->networkState = QStringLiteral("wireless");
    }
}

ControlCenterRuntimeSnapshot collectControlCenterRuntimeSnapshot(bool debugSimulatedBrightnessAvailable,
                                                                int debugSimulatedBrightnessPercentage,
                                                                bool debugSimulatedBatteryAvailable,
                                                                int debugSimulatedBatteryPercentage,
                                                                bool debugSimulatedBatteryCharging,
                                                                bool debugSimulatedBatteryOnAcPower)
{
    ControlCenterRuntimeSnapshot snapshot;

    collectControlCenterNetworkState(&snapshot);

    snapshot.bluetoothAvailable = MauiKitSystem::controlCenterBluetoothAvailable();
    snapshot.bluetoothConnectedDeviceCount = MauiKitSystem::controlCenterBluetoothConnectedDeviceCount();
    snapshot.bluetoothEnabled = snapshot.bluetoothAvailable && MauiKitSystem::controlCenterBluetoothEnabled();

    if (!MauiKitSystem::currentControlCenterVolumeState(&snapshot.volumePercentage, &snapshot.volumeMuted))
    {
        snapshot.volumePercentage = QStringLiteral("0%");
        snapshot.volumeMuted = false;
    }

    const MicrophoneSnapshot microphone = collectMicrophoneSnapshot();
    snapshot.microphoneVolumePercentage = microphone.volumePercentage;
    snapshot.microphoneAvailable = microphone.available;

    if (debugSimulatedBrightnessAvailable)
    {
        snapshot.brightnessAvailable = true;
        snapshot.brightnessPercentage = QStringLiteral("%1%").arg(qBound(0, debugSimulatedBrightnessPercentage, 100));
    }
    else
    {
        snapshot.brightnessAvailable = MauiKitSystem::currentControlCenterBrightnessPercent(&snapshot.brightnessPercentage);
        if (!snapshot.brightnessAvailable)
            snapshot.brightnessPercentage = QStringLiteral("0%");
    }

    if (debugSimulatedBatteryAvailable)
    {
        snapshot.batteryAvailable = true;
        snapshot.batteryOnAcPower = debugSimulatedBatteryOnAcPower || debugSimulatedBatteryCharging;
        snapshot.batteryCharging = debugSimulatedBatteryCharging;
        snapshot.batteryPercentage = QStringLiteral("%1%").arg(qBound(0, debugSimulatedBatteryPercentage, 100));
    }
    else
    {
        QString batteryPath;
        bool mainsOnline = false;
        snapshot.batteryAvailable = MauiKitSystem::batteryPowerSupplyState(&batteryPath, &mainsOnline);

        if (snapshot.batteryAvailable)
        {
            QString capacityText = QStringLiteral("0");
            QString statusText;
            MauiKitSystem::readBatteryCharge(batteryPath, &capacityText, &statusText);

            const int percent = MauiKitSystem::parseBatteryPercent(capacityText);
            const QString normalizedStatus = statusText.toLower();
            snapshot.batteryCharging = normalizedStatus == QLatin1String("charging");
            const bool statusImpliesAc = normalizedStatus == QLatin1String("charging")
                    || normalizedStatus == QLatin1String("full")
                    || normalizedStatus == QLatin1String("not charging");
            snapshot.batteryOnAcPower = mainsOnline || statusImpliesAc;
            snapshot.batteryPercentage = QStringLiteral("%1%").arg(percent);
        }
        else
        {
            snapshot.batteryPercentage = QStringLiteral("0%");
        }
    }

    snapshot.nightLightAvailable = MauiKitSystem::controlCenterNightLightState(&snapshot.nightLightEnabled);

    snapshot.powerProfileCurrentValid = MauiKitSystem::currentPowerProfile(&snapshot.powerProfileCurrent);
    snapshot.powerProfiles = MauiKitSystem::powerProfilesFromPowerProfilesCtl();

    return snapshot;
}

ControlCenterSystemResourcesSnapshot collectControlCenterSystemResourcesSnapshot(const QString &diskUsagePath)
{
    ControlCenterSystemResourcesSnapshot snapshot;

    snapshot.cpuValid = MauiKitSystem::readCpuUsagePercent(&snapshot.cpuPercentage);
    snapshot.ramValid = MauiKitSystem::readRamUsagePercent(&snapshot.ramPercentage);
    snapshot.diskValid = MauiKitSystem::readDiskUsage(diskUsagePath, &snapshot.diskText, &snapshot.diskPercentage);

    return snapshot;
}
}

void ValenzBridge::initializeControlCenterRuntime()
{
    if (!m_controlCenterStatusTimer)
    {
        m_controlCenterStatusTimer = new QTimer(this);
        m_controlCenterStatusTimer->setTimerType(Qt::CoarseTimer);
        connect(m_controlCenterStatusTimer, &QTimer::timeout, this, &ValenzBridge::refreshControlCenterRuntimeState);
    }

    updateControlCenterRuntimeTimer();
    refreshControlCenterRuntimeState();
}

void ValenzBridge::setControlCenterRuntimeActive(bool active)
{
    if (m_controlCenterRuntimeActive == active)
        return;

    m_controlCenterRuntimeActive = active;
    updateControlCenterRuntimeTimer();

    if (m_controlCenterRuntimeActive)
        refreshControlCenterRuntimeState();
}

void ValenzBridge::updateControlCenterRuntimeTimer()
{
    if (!m_controlCenterStatusTimer)
        return;

    const int interval = m_controlCenterRuntimeActive
            ? kControlCenterActiveRefreshIntervalMs
            : kControlCenterIdleRefreshIntervalMs;

    if (m_controlCenterStatusTimer->interval() != interval)
        m_controlCenterStatusTimer->setInterval(interval);

    m_controlCenterStatusTimer->start();
}

void ValenzBridge::refreshControlCenterRuntimeState()
{
    if (m_controlCenterRuntimeRefreshInFlight)
    {
        m_controlCenterRuntimeRefreshPending = true;
        return;
    }

    m_controlCenterRuntimeRefreshInFlight = true;

    const bool debugSimulatedBrightnessAvailable = m_debugSimulatedBrightnessAvailable;
    const int debugSimulatedBrightnessPercentage = m_debugSimulatedBrightnessPercentage;
    const bool debugSimulatedBatteryAvailable = m_debugSimulatedBatteryAvailable;
    const int debugSimulatedBatteryPercentage = m_debugSimulatedBatteryPercentage;
    const bool debugSimulatedBatteryCharging = m_debugSimulatedBatteryCharging;
    const bool debugSimulatedBatteryOnAcPower = m_debugSimulatedBatteryOnAcPower;
    QPointer<ValenzBridge> bridge(this);

    QThreadPool::globalInstance()->start([bridge,
                       debugSimulatedBrightnessAvailable,
                       debugSimulatedBrightnessPercentage,
                       debugSimulatedBatteryAvailable,
                       debugSimulatedBatteryPercentage,
                       debugSimulatedBatteryCharging,
                       debugSimulatedBatteryOnAcPower]() {
        const ControlCenterRuntimeSnapshot snapshot = collectControlCenterRuntimeSnapshot(debugSimulatedBrightnessAvailable,
                                                                                          debugSimulatedBrightnessPercentage,
                                                                                          debugSimulatedBatteryAvailable,
                                                                                          debugSimulatedBatteryPercentage,
                                                                                          debugSimulatedBatteryCharging,
                                                                                          debugSimulatedBatteryOnAcPower);
        if (!bridge)
            return;

        QMetaObject::invokeMethod(bridge.data(), [bridge, snapshot]() {
            if (!bridge)
                return;

            bridge->m_controlCenterRuntimeRefreshInFlight = false;
            bridge->setControlCenterWirelessAvailable(snapshot.wirelessAvailable);
            if (bridge->m_controlCenterWirelessEnabled != snapshot.wirelessEnabled)
            {
                bridge->m_controlCenterWirelessEnabled = snapshot.wirelessEnabled;
                Q_EMIT bridge->controlCenterWirelessEnabledChanged(bridge->m_controlCenterWirelessEnabled);
            }
            if (bridge->m_controlCenterWirelessConnected != snapshot.wirelessConnected)
            {
                bridge->m_controlCenterWirelessConnected = snapshot.wirelessConnected;
                Q_EMIT bridge->controlCenterWirelessConnectedChanged(bridge->m_controlCenterWirelessConnected);
            }
            if (bridge->m_controlCenterWiredConnected != snapshot.wiredConnected)
            {
                bridge->m_controlCenterWiredConnected = snapshot.wiredConnected;
                Q_EMIT bridge->controlCenterWiredConnectedChanged(bridge->m_controlCenterWiredConnected);
            }
            bridge->setControlCenterNetworkState(snapshot.networkState);
            bridge->setControlCenterBluetoothAvailable(snapshot.bluetoothAvailable);
            bridge->setControlCenterBluetoothConnectedDeviceCount(snapshot.bluetoothConnectedDeviceCount);
            if (bridge->m_controlCenterBluetoothEnabled != snapshot.bluetoothEnabled)
            {
                bridge->m_controlCenterBluetoothEnabled = snapshot.bluetoothEnabled;
                Q_EMIT bridge->controlCenterBluetoothEnabledChanged(bridge->m_controlCenterBluetoothEnabled);
            }
            bridge->setControlCenterBluetoothState(snapshot.bluetoothEnabled ? QStringLiteral("on") : QStringLiteral("off"));
            bridge->setControlCenterVolumeMuted(snapshot.volumeMuted);
            bridge->setControlCenterVolumePercentage(snapshot.volumePercentage);
            bridge->setControlCenterMicrophoneVolumePercentage(snapshot.microphoneVolumePercentage);
            bridge->setControlCenterMicrophoneAvailable(snapshot.microphoneAvailable);
            bridge->setControlCenterBrightnessAvailable(snapshot.brightnessAvailable);
            bridge->setControlCenterBrightnessPercentage(snapshot.brightnessPercentage);
            bridge->setControlCenterBatteryAvailable(snapshot.batteryAvailable);
            bridge->setControlCenterBatteryOnAcPower(snapshot.batteryOnAcPower);
            bridge->setControlCenterBatteryCharging(snapshot.batteryCharging);
            bridge->setControlCenterBatteryPercentage(snapshot.batteryPercentage);
            bridge->setControlCenterNightLightAvailable(snapshot.nightLightAvailable);
            if (bridge->m_controlCenterNightLightEnabled != snapshot.nightLightEnabled)
            {
                bridge->m_controlCenterNightLightEnabled = snapshot.nightLightEnabled;
                Q_EMIT bridge->controlCenterNightLightEnabledChanged(bridge->m_controlCenterNightLightEnabled);
            }

            if (!snapshot.powerProfiles.isEmpty())
                bridge->setControlCenterPowerProfiles(snapshot.powerProfiles);

            if (snapshot.powerProfileCurrentValid)
                bridge->updateControlCenterPowerProfileCurrentFromSystem(snapshot.powerProfileCurrent);

            if (bridge->m_controlCenterRuntimeRefreshPending)
            {
                bridge->m_controlCenterRuntimeRefreshPending = false;
                bridge->refreshControlCenterRuntimeState();
            }
        }, Qt::QueuedConnection);
    });
}

void ValenzBridge::refreshControlCenterNetworkState()
{
    ControlCenterRuntimeSnapshot snapshot;
    collectControlCenterNetworkState(&snapshot);

    setControlCenterWirelessAvailable(snapshot.wirelessAvailable);
    if (m_controlCenterWirelessEnabled != snapshot.wirelessEnabled)
    {
        m_controlCenterWirelessEnabled = snapshot.wirelessEnabled;
        Q_EMIT controlCenterWirelessEnabledChanged(m_controlCenterWirelessEnabled);
    }
    if (m_controlCenterWirelessConnected != snapshot.wirelessConnected)
    {
        m_controlCenterWirelessConnected = snapshot.wirelessConnected;
        Q_EMIT controlCenterWirelessConnectedChanged(m_controlCenterWirelessConnected);
    }
    if (m_controlCenterWiredConnected != snapshot.wiredConnected)
    {
        m_controlCenterWiredConnected = snapshot.wiredConnected;
        Q_EMIT controlCenterWiredConnectedChanged(m_controlCenterWiredConnected);
    }
    setControlCenterNetworkState(snapshot.networkState);
}

void ValenzBridge::refreshControlCenterBluetoothState()
{
    const bool available = MauiKitSystem::controlCenterBluetoothAvailable();
    setControlCenterBluetoothAvailable(available);
    setControlCenterBluetoothConnectedDeviceCount(MauiKitSystem::controlCenterBluetoothConnectedDeviceCount());
    if (!available)
    {
        setControlCenterBluetoothEnabled(false);
        return;
    }

    setControlCenterBluetoothEnabled(MauiKitSystem::controlCenterBluetoothEnabled());
}

void ValenzBridge::refreshControlCenterVolumeState()
{
    QString percentText;
    bool muted = false;
    if (!MauiKitSystem::currentControlCenterVolumeState(&percentText, &muted))
    {
        setControlCenterVolumeMuted(false);
        setControlCenterVolumePercentage(QStringLiteral("0%"));
        return;
    }

    setControlCenterVolumeMuted(muted);
    setControlCenterVolumePercentage(percentText);
}

void ValenzBridge::setControlCenterVolumePercentageFromSlider(int percent)
{
    if (MauiKitSystem::setControlCenterVolumePercent(percent))
        refreshControlCenterVolumeState();
}

void ValenzBridge::setControlCenterMicrophoneVolumePercentageFromSlider(int percent)
{
    if (!m_controlCenterMicrophoneAvailable)
        return;

    const bool changed = MauiKitSystem::runCommandText(QStringLiteral("wpctl"),
                                                       QStringList { QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@"), QStringLiteral("%1%").arg(qBound(0, percent, 100)) },
                                                       nullptr, 1000);
    if (changed)
    {
        QString output;
        if (MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList { QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@") }, &output, 1000))
        {
            const QStringList fields = output.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() > 1)
                setControlCenterMicrophoneVolumePercentage(QStringLiteral("%1%").arg(qBound(0, qRound(fields.at(1).toDouble()), 100)));
        }
    }
}

void ValenzBridge::setControlCenterBrightnessPercentageFromSlider(int percent)
{
    MauiKitSystem::setControlCenterBrightnessPercent(percent);
    refreshControlCenterBrightnessState();
}

void ValenzBridge::setControlCenterNightLightEnabled(bool enabled)
{
    bool actualEnabled = false;
    const bool available = MauiKitSystem::controlCenterNightLightState(&actualEnabled);
    setControlCenterNightLightAvailable(available);

    if (!available)
    {
        const bool changed = m_controlCenterNightLightEnabled;
        m_controlCenterNightLightEnabled = false;
        if (changed || enabled)
            Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
        return;
    }

    if (actualEnabled != enabled)
    {
        const bool succeeded = enabled
                ? MauiKitSystem::startControlCenterNightLight()
                : MauiKitSystem::stopControlCenterNightLight();
        if (!succeeded)
        {
            refreshControlCenterNightLightState();
            return;
        }
    }

    if (m_controlCenterNightLightEnabled != enabled)
    {
        m_controlCenterNightLightEnabled = enabled;
        Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
    }

    QTimer::singleShot(250, this, &ValenzBridge::refreshControlCenterNightLightState);
}

void ValenzBridge::executeControlCenterPowerCommand()
{
    const QString command = normalizePowerCommand(m_controlCenterPowerCommand);
    if (!startDetachedCommand(command)
        && command.compare(QStringLiteral("wlogout"), Qt::CaseInsensitive) != 0)
    {
        startDetachedCommand(QStringLiteral("wlogout"));
    }
}

void ValenzBridge::executeControlCenterSettingsCommand()
{
    const QString command = m_controlCenterSettingsCommand.trimmed().isEmpty()
            ? QStringLiteral("systemsettings")
            : m_controlCenterSettingsCommand.trimmed();
    if (!startDetachedCommand(command)
        && !startDetachedCommand(QStringLiteral("systemsettings")))
    {
        startDetachedCommand(QStringLiteral("maui-settings"));
    }
}

void ValenzBridge::refreshControlCenterNightLightState()
{
    bool enabled = false;
    const bool available = MauiKitSystem::controlCenterNightLightState(&enabled);
    setControlCenterNightLightAvailable(available);
    if (!available)
    {
        if (m_controlCenterNightLightEnabled)
        {
            m_controlCenterNightLightEnabled = false;
            Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
        }
        return;
    }

    if (m_controlCenterNightLightEnabled != enabled)
    {
        m_controlCenterNightLightEnabled = enabled;
        Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
    }
}

void ValenzBridge::refreshControlCenterBrightnessState()
{
    if (m_debugSimulatedBrightnessAvailable)
    {
        setControlCenterBrightnessAvailable(true);
        setControlCenterBrightnessPercentage(QStringLiteral("%1%").arg(qBound(0, m_debugSimulatedBrightnessPercentage, 100)));
        return;
    }

    QString percentText;
    const bool available = MauiKitSystem::currentControlCenterBrightnessPercent(&percentText);
    setControlCenterBrightnessAvailable(available);

    if (!available)
    {
        setControlCenterBrightnessPercentage(QStringLiteral("0%"));
        return;
    }

    setControlCenterBrightnessPercentage(percentText);
}

void ValenzBridge::refreshControlCenterSystemResources()
{
    if (m_controlCenterSystemResourcesRefreshInFlight)
    {
        m_controlCenterSystemResourcesRefreshPending = true;
        return;
    }

    m_controlCenterSystemResourcesRefreshInFlight = true;

    const QString diskUsagePath = m_controlCenterDiskUsagePath;
    QPointer<ValenzBridge> bridge(this);

    QThreadPool::globalInstance()->start([bridge, diskUsagePath]() {
        const ControlCenterSystemResourcesSnapshot snapshot = collectControlCenterSystemResourcesSnapshot(diskUsagePath);
        if (!bridge)
            return;

        QMetaObject::invokeMethod(bridge.data(), [bridge, snapshot]() {
            if (!bridge)
                return;

            bridge->m_controlCenterSystemResourcesRefreshInFlight = false;
            if (snapshot.cpuValid)
                bridge->setControlCenterCpuPercentage(snapshot.cpuPercentage);
            if (snapshot.ramValid)
                bridge->setControlCenterRamPercentage(snapshot.ramPercentage);
            if (snapshot.diskValid)
            {
                bridge->setControlCenterDiskUsageText(snapshot.diskText);
                bridge->setControlCenterDiskUsagePercentage(snapshot.diskPercentage);
            }
            else
            {
                bridge->setControlCenterDiskUsageText(QString());
                bridge->setControlCenterDiskUsagePercentage(0);
            }

            if (bridge->m_controlCenterSystemResourcesRefreshPending)
            {
                bridge->m_controlCenterSystemResourcesRefreshPending = false;
                bridge->refreshControlCenterSystemResources();
            }
        }, Qt::QueuedConnection);
    });
}

void ValenzBridge::refreshControlCenterSystemResourcesState()
{
    int cpuPercent = 0;
    if (MauiKitSystem::readCpuUsagePercent(&cpuPercent))
        setControlCenterCpuPercentage(cpuPercent);

    int ramPercent = 0;
    if (MauiKitSystem::readRamUsagePercent(&ramPercent))
        setControlCenterRamPercentage(ramPercent);

    QString diskText;
    int diskPercent = 0;
    if (MauiKitSystem::readDiskUsage(m_controlCenterDiskUsagePath, &diskText, &diskPercent))
    {
        setControlCenterDiskUsageText(diskText);
        setControlCenterDiskUsagePercentage(diskPercent);
    }
    else
    {
        setControlCenterDiskUsageText(QString());
        setControlCenterDiskUsagePercentage(0);
    }
}

void ValenzBridge::refreshControlCenterBatteryState()
{
    if (m_debugSimulatedBatteryAvailable)
    {
        setControlCenterBatteryAvailable(true);
        setControlCenterBatteryOnAcPower(m_debugSimulatedBatteryOnAcPower || m_debugSimulatedBatteryCharging);
        setControlCenterBatteryCharging(m_debugSimulatedBatteryCharging);
        setControlCenterBatteryPercentage(QStringLiteral("%1%").arg(qBound(0, m_debugSimulatedBatteryPercentage, 100)));
        return;
    }

    QString batteryPath;
    bool mainsOnline = false;
    const bool hasBattery = MauiKitSystem::batteryPowerSupplyState(&batteryPath, &mainsOnline);

    if (!hasBattery)
    {
        setControlCenterBatteryAvailable(false);
        setControlCenterBatteryOnAcPower(false);
        setControlCenterBatteryCharging(false);
        setControlCenterBatteryPercentage(QStringLiteral("0%"));
        return;
    }

    setControlCenterBatteryAvailable(true);

    QString capacityText = QStringLiteral("0");
    QString statusText;
    MauiKitSystem::readBatteryCharge(batteryPath, &capacityText, &statusText);

    const int percent = MauiKitSystem::parseBatteryPercent(capacityText);
    const QString normalizedStatus = statusText.toLower();

    const bool charging = normalizedStatus == QLatin1String("charging");
    const bool statusImpliesAc = normalizedStatus == QLatin1String("charging")
                                 || normalizedStatus == QLatin1String("full")
                                 || normalizedStatus == QLatin1String("not charging");
    const bool onAcPower = mainsOnline || statusImpliesAc;

    setControlCenterBatteryOnAcPower(onAcPower);
    setControlCenterBatteryCharging(charging);
    setControlCenterBatteryPercentage(QStringLiteral("%1%").arg(percent));
}

void ValenzBridge::refreshControlCenterPowerProfileState()
{
    QString current;
    if (MauiKitSystem::currentPowerProfile(&current))
        updateControlCenterPowerProfileCurrentFromSystem(current);

    const QStringList profiles = MauiKitSystem::powerProfilesFromPowerProfilesCtl();
    if (!profiles.isEmpty())
        setControlCenterPowerProfiles(profiles);
}
