#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QDebug>

void ValenzBridge::initializeControlCenterRuntime()
{
    if (!m_controlCenterStatusTimer)
    {
        m_controlCenterStatusTimer = new QTimer(this);
        m_controlCenterStatusTimer->setInterval(1000);
        m_controlCenterStatusTimer->setTimerType(Qt::CoarseTimer);
        connect(m_controlCenterStatusTimer, &QTimer::timeout, this, &ValenzBridge::refreshControlCenterRuntimeState);
    }

    if (!m_controlCenterStatusTimer->isActive())
        m_controlCenterStatusTimer->start();

    refreshControlCenterRuntimeState();
}

void ValenzBridge::refreshControlCenterRuntimeState()
{
    refreshControlCenterNetworkState();
    refreshControlCenterBluetoothState();
    refreshControlCenterVolumeState();
    refreshControlCenterBrightnessState();
    refreshControlCenterBatteryState();
    refreshControlCenterNightLightState();
    refreshControlCenterPowerProfileState();
}

void ValenzBridge::refreshControlCenterNetworkState()
{
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
    const bool ok = MauiKitSystem::setControlCenterBrightnessPercent(percent);
    qDebug().noquote() << "ValenzBridge::setControlCenterBrightnessPercentageFromSlider" << percent << ok;
    refreshControlCenterBrightnessState();
}

void ValenzBridge::setControlCenterNightLightEnabled(bool enabled)
{
    if (!MauiKitSystem::controlCenterNightLightAvailable())
    {
        if (m_controlCenterNightLightEnabled && !enabled)
        {
            qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled forcing off; hyprsunset unavailable";
            m_controlCenterNightLightEnabled = false;
            Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
        }
        else
        {
            qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled ignored; hyprsunset unavailable";
        }
        return;
    }

    if (m_controlCenterNightLightEnabled == enabled)
        return;

    qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled" << m_controlCenterNightLightEnabled << "->" << enabled;

    if (enabled)
    {
        const bool started = MauiKitSystem::startControlCenterNightLight();
        qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled start hyprsunset" << started;
        if (!started)
            return;
    }
    else
    {
        const bool stopped = MauiKitSystem::stopControlCenterNightLight();
        qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled stop hyprsunset" << stopped;
    }

    m_controlCenterNightLightEnabled = enabled;
    Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
}

void ValenzBridge::executeControlCenterPowerCommand()
{
    const QString command = normalizePowerCommand(m_controlCenterPowerCommand);
    MauiKitSystem::executeControlCenterPowerCommand(command);
}

void ValenzBridge::refreshControlCenterNightLightState()
{
    const bool available = MauiKitSystem::controlCenterNightLightAvailable();
    setControlCenterNightLightAvailable(available);

    if (!available)
    {
        setControlCenterNightLightEnabled(false);
        return;
    }

    setControlCenterNightLightEnabled(MauiKitSystem::controlCenterNightLightRunning());
}

void ValenzBridge::refreshControlCenterBrightnessState()
{
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
    refreshControlCenterSystemResourcesState();
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
        setControlCenterPowerProfileCurrent(current);

    const QStringList profiles = MauiKitSystem::powerProfilesFromPowerProfilesCtl();
    if (!profiles.isEmpty())
        setControlCenterPowerProfiles(profiles);
}
