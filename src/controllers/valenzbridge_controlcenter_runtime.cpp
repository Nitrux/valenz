#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QTextStream>
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
    double networkUploadRate = 0.0;
    double networkDownloadRate = 0.0;
    bool bluetoothAvailable = false;
    int bluetoothConnectedDeviceCount = 0;
    bool bluetoothEnabled = false;
    QString volumePercentage = QStringLiteral("0%");
    bool volumeMuted = false;
    QString microphoneSource;
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

struct NetworkTrafficSnapshot
{
    double uploadRate = 0.0;
    double downloadRate = 0.0;
};

NetworkTrafficSnapshot collectNetworkTrafficSnapshot()
{
    NetworkTrafficSnapshot snapshot;
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return snapshot;

    QString selectedInterface;
    quint64 selectedReceive = 0;
    quint64 selectedTransmit = 0;
    quint64 largestTraffic = 0;

    QTextStream input(&file);
    input.readLine();
    input.readLine();
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        const qsizetype separator = line.indexOf(QLatin1Char(':'));
        if (separator < 0)
            continue;

        const QString interfaceName = line.left(separator).trimmed();
        if (interfaceName == QStringLiteral("lo"))
            continue;

        QFile stateFile(QStringLiteral("/sys/class/net/") + interfaceName + QStringLiteral("/operstate"));
        if (stateFile.open(QIODevice::ReadOnly))
        {
            const QByteArray state = stateFile.readAll().trimmed();
            if (state != "up" && state != "unknown")
                continue;
        }

        const QStringList fields = line.mid(separator + 1).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 16)
            continue;

        bool receiveOk = false;
        bool transmitOk = false;
        const quint64 receive = fields.at(0).toULongLong(&receiveOk);
        const quint64 transmit = fields.at(8).toULongLong(&transmitOk);
        if (!receiveOk || !transmitOk)
            continue;

        if (receive + transmit >= largestTraffic)
        {
            largestTraffic = receive + transmit;
            selectedInterface = interfaceName;
            selectedReceive = receive;
            selectedTransmit = transmit;
        }
    }

    static QString previousInterface;
    static quint64 previousReceive = 0;
    static quint64 previousTransmit = 0;
    static QElapsedTimer timer;
    if (!timer.isValid())
        timer.start();

    const qint64 elapsed = timer.restart();
    if (selectedInterface == previousInterface && elapsed > 0)
    {
        snapshot.downloadRate = static_cast<double>(selectedReceive >= previousReceive ? selectedReceive - previousReceive : 0) * 1000.0 / elapsed;
        snapshot.uploadRate = static_cast<double>(selectedTransmit >= previousTransmit ? selectedTransmit - previousTransmit : 0) * 1000.0 / elapsed;
    }

    previousInterface = selectedInterface;
    previousReceive = selectedReceive;
    previousTransmit = selectedTransmit;
    return snapshot;
}

struct MicrophoneSnapshot
{
    QString source;
    QString volumePercentage = QStringLiteral("0%");
    bool available = false;
};

bool sourceCanChangeVolume(const QString &source)
{
    QString output;
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), source}, &output, 1000))
    {
        return false;
    }

    const QStringList fields = output.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 2)
        return false;

    bool volumeOk = false;
    const double currentVolume = fields.at(1).toDouble(&volumeOk);
    if (!volumeOk)
    {
        return false;
    }

    const double testVolume = qAbs(currentVolume - 0.50) < 0.005 ? 0.51 : 0.50;
    const QString testValue = QStringLiteral("%1").arg(testVolume, 0, 'f', 2);
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("set-volume"), source, testValue}, nullptr, 1000))
        return false;

    QString afterOutput;
    const bool readBack = MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), source}, &afterOutput, 1000);
    MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("set-volume"), source, QString::number(currentVolume)}, nullptr, 1000);
    if (!readBack)
        return false;

    const QStringList afterFields = afterOutput.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    bool afterOk = false;
    const double afterVolume = afterFields.size() > 1 ? afterFields.at(1).toDouble(&afterOk) : 0.0;
    const bool writable = afterOk && qAbs(afterVolume - testVolume) < 0.005;
    return writable;
}

QString microphoneSourceTarget()
{
    const QString defaultSource = QStringLiteral("@DEFAULT_AUDIO_SOURCE@");
    QString status;
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("status"), QStringLiteral("-n")}, &status, 1000))
    {
        return sourceCanChangeVolume(defaultSource) ? defaultSource : QString();
    }

    QStringList targets;
    bool inSources = false;
    for (const QString &line : status.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        const QString trimmed = line.trimmed();
        if (trimmed.endsWith(QStringLiteral("Sources:")))
        {
            inSources = true;
            continue;
        }
        if (inSources && (trimmed.endsWith(QStringLiteral("Sinks:"))
                          || trimmed.endsWith(QStringLiteral("Filters:"))
                          || trimmed.endsWith(QStringLiteral("Streams:"))))
        {
            inSources = false;
        }
        if (!inSources)
            continue;

        const int dot = line.indexOf(QLatin1Char('.'));
        if (dot <= 0)
            continue;
        QString target = line.left(dot).trimmed();
        const int lastSpace = target.lastIndexOf(QLatin1Char(' '));
        if (lastSpace >= 0)
            target = target.mid(lastSpace + 1);
        if (target.startsWith(QLatin1Char('*')))
            target.remove(0, 1);
        target = target.trimmed();
        bool targetOk = false;
        target.toInt(&targetOk);
        if (targetOk && !targets.contains(target))
        {
            targets.append(target);
        }
    }

    for (const QString &target : targets)
    {
        QString inspect;
        if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("inspect"), target}, &inspect, 1000))
        {
            continue;
        }

        bool availabilityKnown = false;
        bool available = false;
        for (const QString &line : inspect.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        {
            if (!line.contains(QStringLiteral("port.availability"), Qt::CaseInsensitive))
                continue;
            QString value = line.section(QLatin1Char('='), 1).trimmed();
            value.remove(QLatin1Char('"'));
            value = value.toLower();
            availabilityKnown = true;
            available = value == QLatin1String("available");
            break;
        }

        const bool usable = availabilityKnown ? available : sourceCanChangeVolume(target);
        if (usable)
            return target;
    }

    return QString();
}

MicrophoneSnapshot collectMicrophoneSnapshot(const QString &preferredSource)
{
    MicrophoneSnapshot snapshot;
    QString source = preferredSource;
    if (source.isEmpty())
        source = microphoneSourceTarget();
    if (source.isEmpty())
    {
        return snapshot;
    }

    QString volumeOutput;
    if (!MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), source}, &volumeOutput, 1000))
    {
        if (!preferredSource.isEmpty())
        {
            source = microphoneSourceTarget();
            if (source.isEmpty() || !MauiKitSystem::runCommandText(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), source}, &volumeOutput, 1000))
                return snapshot;
        }
        else
            return snapshot;
    }

    const QStringList volumeFields = volumeOutput.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (volumeFields.size() < 2)
        return snapshot;
    bool volumeOk = false;
    const double currentVolume = volumeFields.at(1).toDouble(&volumeOk);
    if (!volumeOk)
        return snapshot;

    snapshot.volumePercentage = QStringLiteral("%1%").arg(qBound(0, qRound(currentVolume * 100.0), 150));
    snapshot.source = source;
    snapshot.available = true;
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

ControlCenterRuntimeSnapshot collectControlCenterRuntimeSnapshot(const QString &preferredMicrophoneSource,
                                                                bool debugSimulatedBrightnessAvailable,
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

    const NetworkTrafficSnapshot networkTraffic = collectNetworkTrafficSnapshot();
    snapshot.networkUploadRate = networkTraffic.uploadRate;
    snapshot.networkDownloadRate = networkTraffic.downloadRate;

    const MicrophoneSnapshot microphone = collectMicrophoneSnapshot(preferredMicrophoneSource);
    snapshot.microphoneSource = microphone.source;
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

    const QString preferredMicrophoneSource = m_controlCenterMicrophoneSource;
    const bool debugSimulatedBrightnessAvailable = m_debugSimulatedBrightnessAvailable;
    const int debugSimulatedBrightnessPercentage = m_debugSimulatedBrightnessPercentage;
    const bool debugSimulatedBatteryAvailable = m_debugSimulatedBatteryAvailable;
    const int debugSimulatedBatteryPercentage = m_debugSimulatedBatteryPercentage;
    const bool debugSimulatedBatteryCharging = m_debugSimulatedBatteryCharging;
    const bool debugSimulatedBatteryOnAcPower = m_debugSimulatedBatteryOnAcPower;
    QPointer<ValenzBridge> bridge(this);

    QThreadPool::globalInstance()->start([bridge,
                       preferredMicrophoneSource,
                       debugSimulatedBrightnessAvailable,
                       debugSimulatedBrightnessPercentage,
                       debugSimulatedBatteryAvailable,
                       debugSimulatedBatteryPercentage,
                       debugSimulatedBatteryCharging,
                       debugSimulatedBatteryOnAcPower]() {
        const ControlCenterRuntimeSnapshot snapshot = collectControlCenterRuntimeSnapshot(preferredMicrophoneSource,
                                                                                          debugSimulatedBrightnessAvailable,
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
            if (!qFuzzyCompare(bridge->m_controlCenterNetworkUploadRate, snapshot.networkUploadRate))
            {
                bridge->m_controlCenterNetworkUploadRate = snapshot.networkUploadRate;
                Q_EMIT bridge->controlCenterNetworkUploadRateChanged(bridge->m_controlCenterNetworkUploadRate);
            }
            if (!qFuzzyCompare(bridge->m_controlCenterNetworkDownloadRate, snapshot.networkDownloadRate))
            {
                bridge->m_controlCenterNetworkDownloadRate = snapshot.networkDownloadRate;
                Q_EMIT bridge->controlCenterNetworkDownloadRateChanged(bridge->m_controlCenterNetworkDownloadRate);
            }
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
            if (!snapshot.microphoneSource.isEmpty())
                bridge->m_controlCenterMicrophoneSource = snapshot.microphoneSource;
            bridge->setControlCenterMicrophoneVolumePercentage(snapshot.microphoneVolumePercentage);
            bridge->setControlCenterMicrophoneAvailable(snapshot.microphoneAvailable);
            bridge->setControlCenterBrightnessAvailable(snapshot.brightnessAvailable);
            bridge->setControlCenterBrightnessPercentage(snapshot.brightnessPercentage);
            bridge->setControlCenterBatteryAvailable(snapshot.batteryAvailable);
            bridge->setControlCenterBatteryOnAcPower(snapshot.batteryOnAcPower);
            bridge->setControlCenterBatteryCharging(snapshot.batteryCharging);
            bridge->setControlCenterBatteryPercentage(snapshot.batteryPercentage);
            bridge->setControlCenterNightLightAvailable(snapshot.nightLightAvailable);
            if (!bridge->m_controlCenterNightLightCommandPending
                && bridge->m_controlCenterNightLightEnabled != snapshot.nightLightEnabled)
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

    const QString source = !m_controlCenterMicrophoneSource.isEmpty()
            ? m_controlCenterMicrophoneSource
            : microphoneSourceTarget();
    if (source.isEmpty())
    {
        return;
    }

    const bool changed = MauiKitSystem::runCommandText(QStringLiteral("wpctl"),
                                                       QStringList { QStringLiteral("set-volume"), source, QStringLiteral("%1%").arg(qBound(0, percent, 100)) },
                                                       nullptr, 1000);
    if (changed)
    {
        QString output;
        if (MauiKitSystem::runCommandText(QStringLiteral("wpctl"), QStringList { QStringLiteral("get-volume"), source }, &output, 1000))
        {
            const QStringList fields = output.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() > 1)
            {
                setControlCenterMicrophoneVolumePercentage(QStringLiteral("%1%").arg(qBound(0, qRound(fields.at(1).toDouble()), 100)));
            }
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
        m_controlCenterNightLightCommandPending = false;
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
    m_controlCenterNightLightCommandPending = actualEnabled != enabled;
    m_controlCenterNightLightCommandTarget = enabled;
    m_controlCenterNightLightPendingChecks = 0;

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
    if (m_controlCenterNightLightCommandPending && available && enabled != m_controlCenterNightLightCommandTarget)
    {
        if (++m_controlCenterNightLightPendingChecks < 20)
        {
            QTimer::singleShot(100, this, &ValenzBridge::refreshControlCenterNightLightState);
            return;
        }
        m_controlCenterNightLightCommandPending = false;
    }
    if (m_controlCenterNightLightCommandPending && available && enabled == m_controlCenterNightLightCommandTarget)
        m_controlCenterNightLightCommandPending = false;

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
