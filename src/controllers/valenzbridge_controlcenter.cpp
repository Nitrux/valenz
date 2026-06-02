#include "valenzbridge.h"
#include "valenzbridge_p.h"

#include <QRegularExpression>

namespace
{
bool runCommandText(const QString &program,
                    const QStringList &arguments,
                    QString *stdOut,
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
    refreshControlCenterBatteryState();
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

void ValenzBridge::executeControlCenterPowerCommand()
{
    const QString command = normalizePowerCommand(m_controlCenterPowerCommand);
    if (QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList { QStringLiteral("-lc"), command }))
        return;

    if (command.compare(QStringLiteral("wlogout"), Qt::CaseInsensitive) != 0)
        QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList { QStringLiteral("-lc"), QStringLiteral("wlogout") });
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
