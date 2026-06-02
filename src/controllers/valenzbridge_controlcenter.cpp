#include "valenzbridge.h"
#include "valenzbridge_p.h"

#include <QRegularExpression>
#include <QDebug>

namespace
{
bool runCommandText(const QString &program,
                    const QStringList &arguments,
                    QString *stdOut = nullptr,
                    int timeoutMs = 350)
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(250))
        return false;

    if (!process.waitForFinished(timeoutMs))
    {
        process.kill();
        process.waitForFinished(250);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;

    if (stdOut)
        *stdOut = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    return true;
}

int parseBatteryPercent(const QString &value)
{
    QString numeric = value.trimmed();
    if (numeric.endsWith(QLatin1Char('%')))
        numeric.chop(1);

    bool ok = false;
    const int parsed = numeric.toInt(&ok);
    if (!ok)
        return 0;

    return qBound(0, parsed, 100);
}

QString defaultRouteInterface()
{
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (!routeFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    while (!routeFile.atEnd())
    {
        const QString line = QString::fromLocal8Bit(routeFile.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("Iface")))
            continue;

        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 2)
            continue;

        if (fields.at(1) == QLatin1String("00000000"))
            return fields.at(0).trimmed();
    }

    return {};
}

bool isWirelessInterface(const QString &interfaceName)
{
    if (interfaceName.isEmpty())
        return false;

    const QFileInfo wirelessDir(QStringLiteral("/sys/class/net/%1/wireless").arg(interfaceName));
    if (wirelessDir.exists() && wirelessDir.isDir())
        return true;

    return interfaceName.startsWith(QLatin1String("wl"), Qt::CaseInsensitive);
}

QString networkStateFromNmcliStatus()
{
    QString output;
    if (!runCommandText(QStringLiteral("nmcli"),
                        QStringList { QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE,STATE"), QStringLiteral("device"), QStringLiteral("status") },
                        &output))
    {
        return QStringLiteral("offline");
    }

    bool hasWired = false;
    bool hasWireless = false;

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const QStringList fields = line.split(QLatin1Char(':'));
        if (fields.size() < 3)
            continue;

        const QString type = fields.at(1).trimmed().toLower();
        const QString state = fields.at(2).trimmed().toLower();
        if (!state.startsWith(QStringLiteral("connected")))
            continue;

        if (type == QLatin1String("ethernet"))
            hasWired = true;
        else if (type == QLatin1String("wifi") || type == QLatin1String("wireless"))
            hasWireless = true;
    }

    if (hasWired)
        return QStringLiteral("wired");
    if (hasWireless)
        return QStringLiteral("wireless");

    return QStringLiteral("offline");
}

bool readCpuUsagePercent(int *percent)
{
    QString output;
    if (!runCommandText(QStringLiteral("top"), QStringList { QStringLiteral("-bn1") }, &output, 1000))
        return false;

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        if (!line.contains(QStringLiteral("Cpu(s)"), Qt::CaseInsensitive))
            continue;

        const QRegularExpression idlePattern(QStringLiteral(R"(([0-9]+(?:\.[0-9]+)?)\s*id)"));
        const QRegularExpressionMatch match = idlePattern.match(line);
        if (!match.hasMatch())
            return false;

        bool ok = false;
        const double idle = match.captured(1).toDouble(&ok);
        if (!ok)
            return false;

        if (percent)
            *percent = qBound(0, qRound(100.0 - idle), 100);
        return true;
    }

    return false;
}

bool readRamUsagePercent(int *percent)
{
    QString output;
    if (!runCommandText(QStringLiteral("free"), QStringList(), &output, 1000))
        return false;

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        if (!line.startsWith(QStringLiteral("Mem:")))
            continue;

        const QStringList fields = line.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
        if (fields.size() < 3)
            return false;

        bool totalOk = false;
        bool usedOk = false;
        const double totalKb = fields.at(1).toDouble(&totalOk);
        const double usedKb = fields.at(2).toDouble(&usedOk);
        if (!totalOk || !usedOk || totalKb <= 0.0)
            return false;

        if (percent)
            *percent = qBound(0, qRound((usedKb / totalKb) * 100.0), 100);
        return true;
    }

    return false;
}

bool readDiskUsage(const QString &path, QString *usageText, int *percent)
{
    const QString targetPath = path.trimmed().isEmpty() ? QStringLiteral("/") : path.trimmed();
    QString output;
    if (!runCommandText(QStringLiteral("df"), QStringList { QStringLiteral("-h"), QStringLiteral("--output=used,size,pcent"), targetPath }, &output, 1200))
        return false;

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() < 2)
        return false;

    const QStringList fields = lines.at(1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (fields.size() < 3)
        return false;

    if (usageText)
        *usageText = QStringLiteral("%1 / %2").arg(fields.at(0), fields.at(1));

    if (percent)
    {
        QString p = fields.at(2).trimmed();
        if (p.endsWith(QLatin1Char('%')))
            p.chop(1);
        bool ok = false;
        const int parsed = p.toInt(&ok);
        if (!ok)
            return false;
        *percent = qBound(0, parsed, 100);
    }

    return true;
}

bool processRunning(const QString &processName)
{
    QString output;
    return runCommandText(QStringLiteral("pgrep"), QStringList { QStringLiteral("-x"), processName }, &output, 250);
}

bool commandAvailable(const QString &program)
{
    QString output;
    return runCommandText(QStringLiteral("sh"), QStringList { QStringLiteral("-lc"), QStringLiteral("command -v %1").arg(program) }, &output, 250) && !output.trimmed().isEmpty();
}

bool stopProcessByName(const QString &processName)
{
    if (runCommandText(QStringLiteral("pkill"), QStringList { QStringLiteral("-x"), processName }, nullptr, 350))
        return true;

    return runCommandText(QStringLiteral("killall"), QStringList { processName }, nullptr, 350);
}

bool bluetoothPoweredFromRfkill(bool *available)
{
    if (available)
        *available = false;

    QString output;
    if (!runCommandText(QStringLiteral("rfkill"), QStringList { QStringLiteral("list"), QStringLiteral("bluetooth") }, &output))
        return false;

    if (available)
        *available = true;

    if (output.contains(QStringLiteral("Soft blocked: yes"), Qt::CaseInsensitive))
        return false;
    if (output.contains(QStringLiteral("Hard blocked: yes"), Qt::CaseInsensitive))
        return false;

    return output.contains(QStringLiteral("Soft blocked: no"), Qt::CaseInsensitive)
        || output.contains(QStringLiteral("Hard blocked: no"), Qt::CaseInsensitive);
}

bool systemSupportsBacklightAdjustment()
{
    QDir backlightDir(QStringLiteral("/sys/class/backlight"));
    const QStringList entries = backlightDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return !entries.isEmpty();
}
}

QVariantList ValenzBridge::controlCenterDiskUsageOptions() const
{
    QVariantList options;

    auto appendEntry = [&options](const QJsonObject &device, const auto &appendEntryRef) -> void {
        const QString type = device.value(QStringLiteral("type")).toString().trimmed().toLower();
        const QString mountPoint = device.value(QStringLiteral("mountpoint")).toString().trimmed();
        const QString pathValue = device.value(QStringLiteral("path")).toString().trimmed();
        const QString label = device.value(QStringLiteral("label")).toString().trimmed();

        if (type == QLatin1String("part") && !mountPoint.isEmpty())
        {
            const QString display = label.isEmpty() ? mountPoint : QStringLiteral("%1 (%2)").arg(label, mountPoint);
            options.push_back(QVariantMap {
                {QStringLiteral("path"), mountPoint},
                {QStringLiteral("label"), label.isEmpty() ? mountPoint : label},
                {QStringLiteral("display"), display},
                {QStringLiteral("device"), pathValue},
            });
        }

        const QJsonArray children = device.value(QStringLiteral("children")).toArray();
        for (const QJsonValue &childValue : children)
            appendEntryRef(childValue.toObject(), appendEntryRef);
    };

    QString output;
    if (!runCommandText(QStringLiteral("lsblk"), QStringList { QStringLiteral("-J"), QStringLiteral("-o"), QStringLiteral("PATH,LABEL,MOUNTPOINT,TYPE"), QStringLiteral("-e"), QStringLiteral("7") }, &output, 1500))
    {
        options.push_back(QVariantMap {
            {QStringLiteral("path"), QStringLiteral("/")},
            {QStringLiteral("label"), QStringLiteral("Root")},
            {QStringLiteral("display"), QStringLiteral("Root (/)")},
        });
        return options;
    }

    const QJsonDocument document = QJsonDocument::fromJson(output.toUtf8());
    if (!document.isObject())
        return options;

    const QJsonArray blockDevices = document.object().value(QStringLiteral("blockdevices")).toArray();
    for (const QJsonValue &deviceValue : blockDevices)
        appendEntry(deviceValue.toObject(), appendEntry);

    const QString currentPath = m_controlCenterDiskUsagePath.trimmed().isEmpty() ? QStringLiteral("/") : m_controlCenterDiskUsagePath.trimmed();
    bool hasCurrentPath = false;
    for (const QVariant &entryVariant : options)
    {
        const QVariantMap entry = entryVariant.toMap();
        if (entry.value(QStringLiteral("path")).toString() == currentPath)
        {
            hasCurrentPath = true;
            break;
        }
    }

    if (!hasCurrentPath)
    {
        options.prepend(QVariantMap {
            {QStringLiteral("path"), currentPath},
            {QStringLiteral("label"), QStringLiteral("Current")},
            {QStringLiteral("display"), QStringLiteral("Current (%1)").arg(currentPath)},
        });
    }

    if (options.isEmpty())
    {
        options.push_back(QVariantMap {
            {QStringLiteral("path"), QStringLiteral("/")},
            {QStringLiteral("label"), QStringLiteral("Root")},
            {QStringLiteral("display"), QStringLiteral("Root (/)")},
        });
    }

    return options;
}


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
    const QString iface = defaultRouteInterface();
    if (!iface.isEmpty())
    {
        setControlCenterNetworkState(isWirelessInterface(iface)
                                         ? QStringLiteral("wireless")
                                         : QStringLiteral("wired"));
        return;
    }

    setControlCenterNetworkState(networkStateFromNmcliStatus());
}

void ValenzBridge::refreshControlCenterBluetoothState()
{
    QString listOutput;
    if (!runCommandText(QStringLiteral("bluetoothctl"), QStringList { QStringLiteral("list") }, &listOutput) || listOutput.trimmed().isEmpty())
    {
        setControlCenterBluetoothAvailable(false);
        setControlCenterBluetoothEnabled(false);
        return;
    }

    setControlCenterBluetoothAvailable(true);

    QString showOutput;
    if (runCommandText(QStringLiteral("bluetoothctl"), QStringList { QStringLiteral("show") }, &showOutput))
    {
        setControlCenterBluetoothEnabled(showOutput.contains(QStringLiteral("Powered: yes"), Qt::CaseInsensitive));
        return;
    }

    setControlCenterBluetoothEnabled(false);
}


void ValenzBridge::refreshControlCenterVolumeState()
{
    QString output;
    if (!runCommandText(QStringLiteral("wpctl"),
                        QStringList { QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@") },
                        &output))
    {
        setControlCenterVolumeMuted(false);
        setControlCenterVolumePercentage(QStringLiteral("0%"));
        return;
    }

    const bool muted = output.contains(QStringLiteral("[MUTED]"), Qt::CaseInsensitive);

    QRegularExpressionMatch match = QRegularExpression(QStringLiteral("([0-9]+(?:\\.[0-9]+)?)")).match(output);
    double volumeRatio = 0.0;
    if (match.hasMatch())
        volumeRatio = match.captured(1).toDouble();

    const int percent = qBound(0, static_cast<int>(std::lround(volumeRatio * 100.0)), 100);

    setControlCenterVolumeMuted(muted);
    setControlCenterVolumePercentage(QStringLiteral("%1%").arg(percent));
}

void ValenzBridge::setControlCenterVolumePercentageFromSlider(int percent)
{
    percent = qBound(0, percent, 100);
    if (!commandAvailable(QStringLiteral("wpctl")))
        return;

    const QString percentText = QStringLiteral("%1%").arg(percent);
    const bool ok = runCommandText(QStringLiteral("wpctl"), QStringList { QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), percentText }, nullptr, 1200);
    if (ok)
    {
        if (percent <= 0)
            runCommandText(QStringLiteral("wpctl"), QStringList { QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("1") }, nullptr, 1200);
        else
            runCommandText(QStringLiteral("wpctl"), QStringList { QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("0") }, nullptr, 1200);
    }

    refreshControlCenterVolumeState();
}

void ValenzBridge::setControlCenterBrightnessPercentageFromSlider(int percent)
{
    percent = qBound(0, percent, 100);
    if (!m_controlCenterBrightnessAvailable)
        return;

    if (!commandAvailable(QStringLiteral("brightnessctl")))
        return;

    const QString percentText = QStringLiteral("%1%").arg(percent);
    const bool ok = runCommandText(QStringLiteral("brightnessctl"), QStringList { QStringLiteral("set"), percentText, QStringLiteral("--quiet") }, nullptr, 1200);
    qDebug().noquote() << "ValenzBridge::setControlCenterBrightnessPercentageFromSlider" << percentText << ok;
    refreshControlCenterBrightnessState();
}

void ValenzBridge::setControlCenterNightLightEnabled(bool enabled)
{
    if (!m_controlCenterNightLightAvailable)
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
        if (!processRunning(QStringLiteral("hyprsunset")))
        {
            const bool started = QProcess::startDetached(QStringLiteral("hyprsunset"));
            qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled start hyprsunset" << started;
            if (!started)
                return;
        }
    }
    else
    {
        const bool stopped = stopProcessByName(QStringLiteral("hyprsunset"));
        qDebug().noquote() << "ValenzBridge::setControlCenterNightLightEnabled stop hyprsunset" << stopped;
    }

    m_controlCenterNightLightEnabled = enabled;
    Q_EMIT controlCenterNightLightEnabledChanged(m_controlCenterNightLightEnabled);
}

void ValenzBridge::executeControlCenterPowerCommand()
{
    const QString command = normalizePowerCommand(m_controlCenterPowerCommand);
    if (QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList { QStringLiteral("-lc"), command }))
        return;

    if (command.compare(QStringLiteral("wlogout"), Qt::CaseInsensitive) != 0)
        QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList { QStringLiteral("-lc"), QStringLiteral("wlogout") });
}

void ValenzBridge::refreshControlCenterNightLightState()
{
    const bool available = commandAvailable(QStringLiteral("hyprsunset"));
    setControlCenterNightLightAvailable(available);

    if (!available)
    {
        setControlCenterNightLightEnabled(false);
        return;
    }

    setControlCenterNightLightEnabled(processRunning(QStringLiteral("hyprsunset")));
}

void ValenzBridge::refreshControlCenterBrightnessState()
{
    const bool available = commandAvailable(QStringLiteral("brightnessctl")) && systemSupportsBacklightAdjustment();
    setControlCenterBrightnessAvailable(available);

    if (!available)
    {
        setControlCenterBrightnessPercentage(QStringLiteral("0%"));
        return;
    }

    QString output;
    if (!runCommandText(QStringLiteral("brightnessctl"), QStringList { QStringLiteral("info") }, &output, 1200))
    {
        setControlCenterBrightnessAvailable(false);
        setControlCenterBrightnessPercentage(QStringLiteral("0%"));
        return;
    }

    QRegularExpressionMatch match = QRegularExpression(QStringLiteral(R"(([0-9]+(?:\.[0-9]+)?)\s*%)")).match(output);
    if (!match.hasMatch())
    {
        setControlCenterBrightnessAvailable(false);
        setControlCenterBrightnessPercentage(QStringLiteral("0%"));
        return;
    }

    const int percent = qBound(0, static_cast<int>(std::lround(match.captured(1).toDouble())), 100);
    setControlCenterBrightnessPercentage(QStringLiteral("%1%").arg(percent));
}

void ValenzBridge::refreshControlCenterSystemResources()
{
    refreshControlCenterSystemResourcesState();
}

void ValenzBridge::refreshControlCenterSystemResourcesState()
{
    int cpuPercent = 0;
    if (readCpuUsagePercent(&cpuPercent))
        setControlCenterCpuPercentage(cpuPercent);

    int ramPercent = 0;
    if (readRamUsagePercent(&ramPercent))
        setControlCenterRamPercentage(ramPercent);

    QString diskText;
    int diskPercent = 0;
    if (readDiskUsage(m_controlCenterDiskUsagePath, &diskText, &diskPercent))
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
    QDir powerSupplyDir(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = powerSupplyDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QString batteryPath;
    bool mainsOnline = false;

    for (const QString &entry : entries)
    {
        const QString supplyPath = powerSupplyDir.absoluteFilePath(entry);
        const QFileInfo typeInfo(supplyPath + QStringLiteral("/type"));
        if (!typeInfo.exists())
            continue;

        QFile typeFile(typeInfo.filePath());
        if (!typeFile.open(QIODevice::ReadOnly))
            continue;

        const QString type = QString::fromUtf8(typeFile.readAll()).trimmed();

        if (type == QLatin1String("Battery") && batteryPath.isEmpty())
        {
            batteryPath = supplyPath;
            continue;
        }

        if (type == QLatin1String("Mains"))
        {
            QFile onlineFile(supplyPath + QStringLiteral("/online"));
            if (onlineFile.open(QIODevice::ReadOnly))
            {
                const QString online = QString::fromUtf8(onlineFile.readAll()).trimmed();
                mainsOnline = mainsOnline || (online == QLatin1String("1"));
            }
        }
    }

    if (batteryPath.isEmpty())
    {
        setControlCenterBatteryAvailable(false);
        setControlCenterBatteryOnAcPower(false);
        setControlCenterBatteryCharging(false);
        setControlCenterBatteryPercentage(QStringLiteral("0%"));
        return;
    }

    setControlCenterBatteryAvailable(true);

    QString capacityText = QStringLiteral("0");
    QFile capacityFile(batteryPath + QStringLiteral("/capacity"));
    if (capacityFile.open(QIODevice::ReadOnly))
        capacityText = QString::fromUtf8(capacityFile.readAll()).trimmed();

    QString statusText;
    QFile statusFile(batteryPath + QStringLiteral("/status"));
    if (statusFile.open(QIODevice::ReadOnly))
        statusText = QString::fromUtf8(statusFile.readAll()).trimmed();

    const int percent = parseBatteryPercent(capacityText);
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
    if (runCommandText(QStringLiteral("powerprofilesctl"), QStringList { QStringLiteral("get") }, &current))
    {
        const QString normalizedCurrent = current.trimmed().toLower();
        if (!normalizedCurrent.isEmpty())
            setControlCenterPowerProfileCurrent(normalizedCurrent);
    }

    QString listedProfiles;
    if (!runCommandText(QStringLiteral("powerprofilesctl"), QStringList { QStringLiteral("list") }, &listedProfiles))
        return;

    QStringList profiles;
    const QStringList lines = listedProfiles.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines)
    {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('*')))
            line.remove(0, 1);

        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;

        const QString profileName = line.left(colon).trimmed().toLower();
        if (profileName.isEmpty() || profiles.contains(profileName))
            continue;

        profiles.push_back(profileName);
    }

    if (!profiles.isEmpty())
        setControlCenterPowerProfiles(profiles);
}
