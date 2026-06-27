#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QThreadPool>

namespace
{
constexpr int kControlCenterIdleRefreshIntervalMs = 3000;
constexpr int kControlCenterActiveRefreshIntervalMs = 1000;

struct ControlCenterRuntimeSnapshot
{
    bool wirelessAvailable = false;
    QString networkState = QStringLiteral("offline");
    bool bluetoothAvailable = false;
    int bluetoothConnectedDeviceCount = 0;
    bool bluetoothEnabled = false;
    QString volumePercentage = QStringLiteral("0%");
    bool volumeMuted = false;
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

ControlCenterRuntimeSnapshot collectControlCenterRuntimeSnapshot(bool debugSimulatedBrightnessAvailable,
                                                                int debugSimulatedBrightnessPercentage,
                                                                bool debugSimulatedBatteryAvailable,
                                                                int debugSimulatedBatteryPercentage,
                                                                bool debugSimulatedBatteryCharging,
                                                                bool debugSimulatedBatteryOnAcPower)
{
    ControlCenterRuntimeSnapshot snapshot;

    snapshot.wirelessAvailable = MauiKitSystem::controlCenterWirelessAvailable();

    const QString iface = MauiKitSystem::defaultRouteInterface();
    if (!iface.isEmpty())
    {
        snapshot.networkState = MauiKitSystem::isWirelessInterface(iface)
                ? QStringLiteral("wireless")
                : QStringLiteral("wired");
    }
    else
    {
        snapshot.networkState = MauiKitSystem::networkStateFromNmcliStatus();
    }

    snapshot.bluetoothAvailable = MauiKitSystem::controlCenterBluetoothAvailable();
    snapshot.bluetoothConnectedDeviceCount = MauiKitSystem::controlCenterBluetoothConnectedDeviceCount();
    snapshot.bluetoothEnabled = snapshot.bluetoothAvailable && MauiKitSystem::controlCenterBluetoothEnabled();

    if (!MauiKitSystem::currentControlCenterVolumeState(&snapshot.volumePercentage, &snapshot.volumeMuted))
    {
        snapshot.volumePercentage = QStringLiteral("0%");
        snapshot.volumeMuted = false;
    }

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

    snapshot.nightLightAvailable = MauiKitSystem::controlCenterNightLightAvailable();
    snapshot.nightLightEnabled = snapshot.nightLightAvailable && MauiKitSystem::controlCenterNightLightRunning();

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
    setControlCenterWirelessAvailable(MauiKitSystem::controlCenterWirelessAvailable());

    const QString iface = MauiKitSystem::defaultRouteInterface();
    if (!iface.isEmpty())
    {
        setControlCenterNetworkState(MauiKitSystem::isWirelessInterface(iface)
                                         ? QStringLiteral("wireless")
                                         : QStringLiteral("wired"));
        return;
    }

    setControlCenterNetworkState(MauiKitSystem::networkStateFromNmcliStatus());
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

void ValenzBridge::setControlCenterBrightnessPercentageFromSlider(int percent)
{
    MauiKitSystem::setControlCenterBrightnessPercent(percent);
    refreshControlCenterBrightnessState();
}

void ValenzBridge::setControlCenterNightLightEnabled(bool enabled)
{
    if (!MauiKitSystem::controlCenterNightLightAvailable())
    {
        if (m_controlCenterNightLightEnabled && !enabled)
        {
            m_controlCenterNightLightEnabled = false;
            Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
        }
        return;
    }

    if (m_controlCenterNightLightEnabled == enabled)
        return;

    if (enabled)
    {
        if (!MauiKitSystem::startControlCenterNightLight())
            return;
    }
    else
    {
        MauiKitSystem::stopControlCenterNightLight();
    }

    m_controlCenterNightLightEnabled = enabled;
    Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
}

void ValenzBridge::executeControlCenterPowerCommand()
{
    const QString command = normalizePowerCommand(m_controlCenterPowerCommand);
    MauiKitSystem::executeControlCenterPowerCommand(command);
}

void ValenzBridge::executeControlCenterSettingsCommand()
{
    const QString command = m_controlCenterSettingsCommand.trimmed().isEmpty()
            ? QStringLiteral("systemsettings")
            : m_controlCenterSettingsCommand.trimmed();
    MauiKitSystem::executeControlCenterSettingsCommand(command);
}

void ValenzBridge::refreshControlCenterNightLightState()
{
    const bool available = MauiKitSystem::controlCenterNightLightAvailable();
    setControlCenterNightLightAvailable(available);

    const bool running = MauiKitSystem::controlCenterNightLightRunning();
    if (!available)
    {
        if (m_controlCenterNightLightEnabled)
        {
            m_controlCenterNightLightEnabled = false;
            Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
        }
        return;
    }

    if (m_controlCenterNightLightEnabled != running)
    {
        m_controlCenterNightLightEnabled = running;
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
