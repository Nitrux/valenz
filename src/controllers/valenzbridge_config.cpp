#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QCryptographicHash>
#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QProcess>
#include <QUrl>

namespace
{
QString kdeGlobalsPath()
{
    return QDir::home().filePath(QStringLiteral(".config/kdeglobals"));
}

QString colorSchemeFilePath(const QString &scheme)
{
    const QString normalized = scheme.trimmed();
    if (normalized.isEmpty())
        return {};

    const QString fileName = normalized.endsWith(QStringLiteral(".colors")) ? normalized : normalized + QStringLiteral(".colors");
    for (const QString &basePath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
    {
        const QString candidate = QDir(basePath).filePath(QStringLiteral("color-schemes/") + fileName);
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return {};
}

void notifyKdePaletteChanged()
{
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"),
                                                       QStringLiteral("org.kde.KGlobalSettings"),
                                                       QStringLiteral("notifyChange"));
    message.setArguments({0, 0});
    QDBusConnection::sessionBus().send(message);
}

QStringList cameraDeviceNodes()
{
    const QDir devDirectory(QStringLiteral("/dev"));
    QStringList devices;
    for (const QString &name : devDirectory.entryList(QStringList {QStringLiteral("video*")}, QDir::System | QDir::Readable, QDir::Name))
        devices.append(devDirectory.filePath(name));
    return devices;
}

QStringList cameraPrivacyDevices()
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("v4l2-ctl"));
    if (executable.isEmpty())
        return {};

    QStringList devices;
    for (const QString &device : cameraDeviceNodes())
    {
        QProcess process;
        process.start(executable, {QStringLiteral("--device"), device, QStringLiteral("--list-ctrls")});
        if (!process.waitForFinished(1000))
            continue;

        const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0
            && output.contains(QStringLiteral("privacy"), Qt::CaseInsensitive))
            devices.append(device);
    }

    return devices;
}

int cameraPrivacyValue(const QString &device)
{
    QProcess process;
    process.start(QStringLiteral("v4l2-ctl"), {QStringLiteral("--device"), device, QStringLiteral("--get-ctrl=privacy")});
    if (!process.waitForFinished(1000) || process.exitCode() != 0)
        return -1;

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const int separator = output.indexOf(QLatin1Char(':'));
    if (separator < 0)
        return -1;

    bool ok = false;
    const int value = output.mid(separator + 1).trimmed().toInt(&ok);
    return ok ? value : -1;
}

bool setCameraPrivacy(const QString &device, bool enabled)
{
    QProcess process;
    process.start(QStringLiteral("v4l2-ctl"), {QStringLiteral("--device"), device, QStringLiteral("--set-ctrl=privacy=") + (enabled ? QStringLiteral("0") : QStringLiteral("1"))});
    return process.waitForFinished(1000)
        && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

bool schemeIsDark(const QString &scheme)
{
    const QString normalized = scheme.toLower();
    return normalized.contains(QStringLiteral("dark"))
        || normalized.contains(QStringLiteral("mocha"))
        || normalized.contains(QStringLiteral("night"));
}

} // namespace

QString ValenzBridge::configFilePath() const
{
    return m_userConfigPath;
}

bool ValenzBridge::darkMode() const
{
    return m_darkMode;
}

void ValenzBridge::setDarkMode(bool enabled)
{
    if (!m_darkModeAvailable)
        return;

    const QString targetScheme = pairedColorScheme(m_colorScheme);
    if (targetScheme.isEmpty() || schemeIsDark(targetScheme) != enabled)
        return;

    if (applyColorScheme(targetScheme))
        refreshSharedSettingsFromFile();
}

bool ValenzBridge::darkModeAvailable() const
{
    return m_darkModeAvailable;
}

QString ValenzBridge::colorScheme() const
{
    return m_colorScheme;
}

bool ValenzBridge::cameraAvailable() const
{
    return m_cameraAvailable;
}

bool ValenzBridge::cameraEnabled() const
{
    return m_cameraEnabled;
}

void ValenzBridge::setCameraEnabled(bool enabled)
{
    const QStringList devices = cameraPrivacyDevices();
    if (devices.isEmpty())
        return;

    bool changed = true;
    for (const QString &device : devices)
        changed = setCameraPrivacy(device, enabled) && changed;

    if (changed)
        refreshSharedSettingsFromFile();
}

void ValenzBridge::refreshSharedSettings()
{
    refreshSharedSettingsFromFile();
}

QString ValenzBridge::pairedColorScheme(const QString &scheme) const
{
    const QSettings settings(m_userConfigPath, QSettings::IniFormat);
    const QString lightScheme = settings.value(QString::fromLatin1(kLightColorSchemeKey), QStringLiteral("CatppuccinLatteNitrux")).toString().trimmed();
    const QString darkScheme = settings.value(QString::fromLatin1(kDarkColorSchemeKey), QStringLiteral("CatppuccinMochaNitrux")).toString().trimmed();
    if (scheme.compare(darkScheme, Qt::CaseInsensitive) == 0)
        return colorSchemeFilePath(lightScheme).isEmpty() ? QString() : lightScheme;
    if (scheme.compare(lightScheme, Qt::CaseInsensitive) == 0)
        return colorSchemeFilePath(darkScheme).isEmpty() ? QString() : darkScheme;
    return {};
}

bool ValenzBridge::applyColorScheme(const QString &scheme)
{
    const QString sourcePath = colorSchemeFilePath(scheme);
    if (sourcePath.isEmpty())
        return false;

    const KSharedConfigPtr target = KSharedConfig::openConfig(kdeGlobalsPath(), KConfig::SimpleConfig);
    const KSharedConfigPtr source = KSharedConfig::openConfig(sourcePath, KConfig::SimpleConfig);
    const QStringList colorGroups {
        QStringLiteral("Colors:View"), QStringLiteral("Colors:Window"), QStringLiteral("Colors:Button"),
        QStringLiteral("Colors:Selection"), QStringLiteral("Colors:Tooltip"), QStringLiteral("Colors:Complementary"),
        QStringLiteral("Colors:Header"), QStringLiteral("ColorEffects:Inactive"), QStringLiteral("ColorEffects:Disabled")
    };

    for (const QString &group : colorGroups)
    {
        KConfigGroup targetGroup(target, group);
        targetGroup.deleteGroup();
        KConfigGroup sourceGroup(source, group);
        if (sourceGroup.exists())
            sourceGroup.copyTo(&targetGroup);
    }

    KConfigGroup sourceWindowManager(source, QStringLiteral("WM"));
    KConfigGroup targetWindowManager(target, QStringLiteral("WM"));
    for (const QString &key : {QStringLiteral("activeBackground"), QStringLiteral("activeForeground"), QStringLiteral("inactiveBackground"), QStringLiteral("inactiveForeground"), QStringLiteral("activeBlend"), QStringLiteral("inactiveBlend")})
    {
        if (sourceWindowManager.hasKey(key))
            targetWindowManager.writeEntry(key, sourceWindowManager.readEntry(key, QString()));
    }

    KConfigGroup sourceKde(source, QStringLiteral("KDE"));
    KConfigGroup targetKde(target, QStringLiteral("KDE"));
    for (const QString &key : {QStringLiteral("frameContrast"), QStringLiteral("contrast")})
    {
        if (sourceKde.hasKey(key))
            targetKde.writeEntry(key, sourceKde.readEntry(key, QString()));
    }

    QFile schemeFile(sourcePath);
    if (schemeFile.open(QIODevice::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(&schemeFile);
        KConfigGroup(target, QStringLiteral("General")).writeEntry(QStringLiteral("ColorSchemeHash"), QString::fromLatin1(hash.result().toHex()));
    }
    KConfigGroup(target, QStringLiteral("General")).writeEntry(QStringLiteral("ColorScheme"), scheme);
    target->sync();

    notifyKdePaletteChanged();
    return true;
}

void ValenzBridge::refreshSharedSettingsFromFile()
{
    const QString path = kdeGlobalsPath();
    const KSharedConfigPtr settings = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    const KConfigGroup generalGroup(settings, QStringLiteral("General"));
    QString scheme = generalGroup.readEntry(QStringLiteral("ColorScheme"), QString());
    if (scheme.isEmpty())
        scheme = settings->group(QStringLiteral("KDE")).readEntry(QStringLiteral("ColorScheme"), QString());
    scheme = scheme.trimmed();
    const bool dark = schemeIsDark(scheme);
    const bool available = !pairedColorScheme(scheme).isEmpty();

    const bool colorChanged = m_colorScheme != scheme;
    const bool darkChanged = m_darkMode != dark || m_darkModeAvailable != available;
    m_colorScheme = scheme;
    m_darkMode = dark;
    m_darkModeAvailable = available;
    if (colorChanged)
        Q_EMIT colorSchemeChanged();
    if (darkChanged)
        Q_EMIT darkModeChanged();

    const QStringList cameraDevices = cameraDeviceNodes();
    const QStringList privacyDevices = cameraPrivacyDevices();
    bool cameraEnabled = !cameraDevices.isEmpty();
    for (const QString &device : privacyDevices)
    {
        const int privacy = cameraPrivacyValue(device);
        if (privacy < 0 || privacy != 0)
            cameraEnabled = false;
    }
    const bool cameraChanged = m_cameraAvailable != !cameraDevices.isEmpty() || m_cameraEnabled != cameraEnabled;
    m_cameraAvailable = !cameraDevices.isEmpty();
    m_cameraEnabled = cameraEnabled;
    if (cameraChanged)
        Q_EMIT cameraStateChanged();

    initializeSharedSettingsWatcher();
}

void ValenzBridge::initializeSharedSettingsWatcher()
{
    const QString configDirectory = QDir::home().filePath(QStringLiteral(".config"));
    const QString configPath = kdeGlobalsPath();
    if (!m_kdeGlobalsWatcher)
    {
        m_kdeGlobalsWatcher = new QFileSystemWatcher(this);
        m_kdeGlobalsReloadTimer = new QTimer(this);
        m_kdeGlobalsReloadTimer->setSingleShot(true);
        m_kdeGlobalsReloadTimer->setInterval(150);
        const auto schedule = [this]() { if (m_kdeGlobalsReloadTimer) m_kdeGlobalsReloadTimer->start(); };
        connect(m_kdeGlobalsWatcher, &QFileSystemWatcher::fileChanged, this, schedule);
        connect(m_kdeGlobalsWatcher, &QFileSystemWatcher::directoryChanged, this, schedule);
        connect(m_kdeGlobalsReloadTimer, &QTimer::timeout, this, &ValenzBridge::refreshSharedSettingsFromFile);
    }
    if (!m_kdeGlobalsWatcher->directories().contains(configDirectory))
        m_kdeGlobalsWatcher->addPath(configDirectory);
    if (QFileInfo::exists(configPath) && !m_kdeGlobalsWatcher->files().contains(configPath))
        m_kdeGlobalsWatcher->addPath(configPath);
}

int ValenzBridge::barHeight() const
{
    return m_barHeight;
}

int ValenzBridge::barLayerSpacing() const
{
    return m_barLayerSpacing;
}

int ValenzBridge::barLayerSpacingTop() const
{
    return m_barLayerSpacingTop;
}

int ValenzBridge::barLayerSpacingBottom() const
{
    return m_barLayerSpacingBottom;
}

int ValenzBridge::barLayerSpacingLeft() const
{
    return m_barLayerSpacingLeft;
}

int ValenzBridge::barLayerSpacingRight() const
{
    return m_barLayerSpacingRight;
}

bool ValenzBridge::systemTrayDebugDetails() const
{
    return m_systemTrayDebugDetails;
}

int ValenzBridge::clampWorkspace(int workspace) const
{
    return qBound(1, workspace, m_workspaceCount);
}

void ValenzBridge::initializeConfig()
{
    const QString configDir = QDir::home().filePath(".config/valenz");
    m_userConfigDirPath = configDir;
    QDir dir;
    dir.mkpath(configDir);

    m_userConfigPath = configDir + "/valenz.conf";

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);

    if (QFileInfo::exists(kDistroConfigPath))
    {
        QSettings distroSettings(QString::fromLatin1(kDistroConfigPath), QSettings::IniFormat);
        const QStringList distroKeys = distroSettings.allKeys();
        for (const QString &key : distroKeys)
        {
            if (!userSettings.contains(key))
            {
                userSettings.setValue(key, distroSettings.value(key));
            }
        }
    }

    const auto ensureKey = [&userSettings](const QString &newKey, const QString &legacyKey, const QVariant &defaultValue)
    {
        if (userSettings.contains(newKey))
            return;

        if (userSettings.contains(legacyKey))
        {
            userSettings.setValue(newKey, userSettings.value(legacyKey));
            userSettings.remove(legacyKey);
            return;
        }

        userSettings.setValue(newKey, defaultValue);
    };
    ensureKey(QString::fromLatin1(kControlCenterIconModeKey), QString::fromLatin1(kLegacyControlCenterIconModeKey), QStringLiteral("system16"));
    ensureKey(QString::fromLatin1(kLightColorSchemeKey), QString(), QStringLiteral("CatppuccinLatteNitrux"));
    ensureKey(QString::fromLatin1(kDarkColorSchemeKey), QString(), QStringLiteral("CatppuccinMochaNitrux"));

    ensureKey(QString::fromLatin1(kWeatherLatitudeKey), QString(), 40.7128);
    ensureKey(QString::fromLatin1(kWeatherLongitudeKey), QString(), -74.0060);
    ensureKey(QString::fromLatin1(kWeatherTemperatureUnitKey), QString(), "celsius");
    ensureKey(QString::fromLatin1(kWeatherRefreshMinutesKey), QString(), 20);
    ensureKey(QString::fromLatin1(kMprisAlwaysVisibleKey), QString(), false);
    ensureKey(QString::fromLatin1(kControlCenterDiskUsagePathKey), QString::fromLatin1(kLegacyControlCenterDiskUsagePathKey), "/");
    ensureKey(QString::fromLatin1(kWindowBarHeightKey), QString(), 56);
    ensureKey(QString::fromLatin1(kWindowBarLayerSpacingKey), QString(), 0);

    userSettings.remove(QStringLiteral("Window/barWidth"));
    userSettings.remove(QStringLiteral("Window/popupMaxWidth"));
    ensureKey(QString::fromLatin1(kWindowBarLayerSpacingTopKey), QString(), 0);
    ensureKey(QString::fromLatin1(kWindowBarLayerSpacingBottomKey), QString(), 0);
    ensureKey(QString::fromLatin1(kWindowBarLayerSpacingLeftKey), QString(), 0);
    ensureKey(QString::fromLatin1(kWindowBarLayerSpacingRightKey), QString(), 0);
    ensureKey(QString::fromLatin1(kWindowScreenPlacementKey), QString(), QStringLiteral("active"));
    ensureKey(QString::fromLatin1(kSystemTrayDebugDetailsKey), QString::fromLatin1(kLegacySystemTrayDebugDetailsKey), false);
    ensureKey(QString::fromLatin1(kDebugSimulatedBrightnessAvailableKey), QString(), false);
    ensureKey(QString::fromLatin1(kDebugSimulatedBrightnessPercentageKey), QString(), 65);
    ensureKey(QString::fromLatin1(kDebugSimulatedBatteryAvailableKey), QString(), false);
    ensureKey(QString::fromLatin1(kDebugSimulatedBatteryPercentageKey), QString(), 72);
    ensureKey(QString::fromLatin1(kDebugSimulatedBatteryChargingKey), QString(), false);
    ensureKey(QString::fromLatin1(kDebugSimulatedBatteryOnAcPowerKey), QString(), false);
    ensureKey(QString::fromLatin1(kControlCenterSettingsCommandKey), QString(), QStringLiteral("systemsettings"));
    ensureKey(QString::fromLatin1(kLauncherCommandKey), QString(), QStringLiteral("vicinae toggle"));
    ensureKey(QString::fromLatin1(kClipboardCommandKey), QString(), QStringLiteral("vicinae vicinae://launch/clipboard/history"));
    userSettings.remove("ControlCenter/batteryIconName");
    userSettings.remove("controlCenter/batteryIconName");
    userSettings.remove("ControlCenter/powerProfileIconName");
    userSettings.remove("Window/focusedWindowTitle");
    userSettings.remove("window/title");

    const QString focusedWindowIconName = userSettings.value(QString::fromLatin1(kFocusedWindowIconNameKey),
                                                              userSettings.value(QString::fromLatin1(kLegacyFocusedWindowIconNameKey),
                                                                                 QStringLiteral("application-x-executable")))
                                               .toString()
                                               .trimmed();
    userSettings.remove(QString::fromLatin1(kFocusedWindowIconNameKey));
    userSettings.remove(QString::fromLatin1(kLegacyFocusedWindowIconNameKey));

    userSettings.sync();
    m_focusedWindowTitle.clear();
    m_focusedWindowIconName = focusedWindowIconName.isEmpty() ? QStringLiteral("application-x-executable") : focusedWindowIconName;
    m_userRealName = MauiKitSystem::systemUserRealName();
    const QString facePath = QDir::home().filePath(QStringLiteral(".face"));
    m_userAvatarPath = QFileInfo::exists(facePath) ? QUrl::fromLocalFile(facePath).toString() : QString();
    m_controlCenterIconMode = normalizeControlCenterIconMode(userSettings.value(kControlCenterIconModeKey, QStringLiteral("system16")).toString());
    m_controlCenterNetworkMode = QStringLiteral("auto");
    m_controlCenterBluetoothState = QStringLiteral("auto");
    m_controlCenterVolumeState = QStringLiteral("auto");
    m_controlCenterPowerProfiles = QStringList { QStringLiteral("power-saver"),
                                                  QStringLiteral("balanced"),
                                                  QStringLiteral("performance") };
    m_controlCenterPowerProfileCurrent = QStringLiteral("balanced");
    m_controlCenterVolumePercentage = QStringLiteral("50%");
    m_controlCenterBatteryCharging = false;
    m_controlCenterBatteryPercentage = QStringLiteral("0%");
    m_controlCenterPowerCommand = normalizePowerCommand(userSettings.value(kControlCenterPowerCommandKey, "wlogout").toString());
    m_controlCenterSettingsCommand = userSettings.value(kControlCenterSettingsCommandKey, QStringLiteral("systemsettings")).toString().trimmed();
    m_launcherCommand = userSettings.value(kLauncherCommandKey, QStringLiteral("vicinae toggle")).toString().trimmed();
    m_clipboardCommand = userSettings.value(kClipboardCommandKey, QStringLiteral("vicinae vicinae://launch/clipboard/history")).toString().trimmed();
    if (m_controlCenterSettingsCommand.isEmpty())
        m_controlCenterSettingsCommand = QStringLiteral("systemsettings");
    if (m_launcherCommand.isEmpty())
        m_launcherCommand = QStringLiteral("vicinae toggle");
    if (m_clipboardCommand.isEmpty())
        m_clipboardCommand = QStringLiteral("vicinae vicinae://launch/clipboard/history");
    m_controlCenterDiskUsagePath = userSettings.value(kControlCenterDiskUsagePathKey, "/").toString().trimmed();
    if (m_controlCenterDiskUsagePath.isEmpty())
        m_controlCenterDiskUsagePath = QStringLiteral("/");
    if (!m_controlCenterDiskUsagePath.startsWith(QLatin1Char('/')))
        m_controlCenterDiskUsagePath.prepend(QLatin1Char('/'));
    m_mprisAlwaysVisible = userSettings.value(kMprisAlwaysVisibleKey, false).toBool();
    m_barHeight = qBound(1, userSettings.value(kWindowBarHeightKey, 56).toInt(), kWindowBarHeightMax);
    m_barLayerSpacing = qBound(0, userSettings.value(kWindowBarLayerSpacingKey, 0).toInt(), 64);
    m_barLayerSpacingTop = qBound(0, userSettings.value(kWindowBarLayerSpacingTopKey, m_barLayerSpacing).toInt(), 64);
    m_barLayerSpacingBottom = qBound(0, userSettings.value(kWindowBarLayerSpacingBottomKey, m_barLayerSpacing).toInt(), 64);
    m_barLayerSpacingLeft = qBound(0, userSettings.value(kWindowBarLayerSpacingLeftKey, m_barLayerSpacing).toInt(), 64);
    m_barLayerSpacingRight = qBound(0, userSettings.value(kWindowBarLayerSpacingRightKey, m_barLayerSpacing).toInt(), 64);
    m_screenPlacement = normalizeScreenPlacement(userSettings.value(kWindowScreenPlacementKey, QStringLiteral("active")).toString());
    userSettings.setValue(kWindowScreenPlacementKey, m_screenPlacement);
    userSettings.sync();
    m_systemTrayDebugDetails = userSettings.value(kSystemTrayDebugDetailsKey, false).toBool();
    m_debugSimulatedBrightnessAvailable = userSettings.value(kDebugSimulatedBrightnessAvailableKey, false).toBool();
    m_debugSimulatedBrightnessPercentage = qBound(0, userSettings.value(kDebugSimulatedBrightnessPercentageKey, 65).toInt(), 100);
    m_debugSimulatedBatteryAvailable = userSettings.value(kDebugSimulatedBatteryAvailableKey, false).toBool();
    m_debugSimulatedBatteryPercentage = qBound(0, userSettings.value(kDebugSimulatedBatteryPercentageKey, 72).toInt(), 100);
    m_debugSimulatedBatteryCharging = userSettings.value(kDebugSimulatedBatteryChargingKey, false).toBool();
    m_debugSimulatedBatteryOnAcPower = userSettings.value(kDebugSimulatedBatteryOnAcPowerKey, false).toBool();

    m_weatherLatitude = normalizeWeatherCoordinate(userSettings.value(kWeatherLatitudeKey, 40.7128), -90.0, 90.0, 40.7128);
    m_weatherLongitude = normalizeWeatherCoordinate(userSettings.value(kWeatherLongitudeKey, -74.0060), -180.0, 180.0, -74.0060);
    m_weatherTemperatureUnit = normalizeWeatherTemperatureUnit(userSettings.value(kWeatherTemperatureUnitKey, "celsius").toString());
    m_weatherRefreshMinutes = normalizeWeatherRefreshMinutes(userSettings.value(kWeatherRefreshMinutesKey, 20));

    m_weatherIconName = QStringLiteral("weather-severe-alert");
    m_weatherTemperature = m_weatherTemperatureUnit == QLatin1String("fahrenheit") ? QStringLiteral("--°F") : QStringLiteral("--°C");
    m_weatherConditionLabel = QString();
    m_weatherLocationName = QString();
}

void ValenzBridge::initializeConfigWatcher()
{
    m_configWatcher = new QFileSystemWatcher(this);
    m_configReloadTimer = new QTimer(this);
    m_configReloadTimer->setSingleShot(true);
    m_configReloadTimer->setInterval(200);

    connect(m_configWatcher, &QFileSystemWatcher::fileChanged, this, &ValenzBridge::scheduleConfigReload);
    connect(m_configWatcher, &QFileSystemWatcher::directoryChanged, this, &ValenzBridge::scheduleConfigReload);
    connect(m_configReloadTimer, &QTimer::timeout, this, &ValenzBridge::reloadConfig);
    refreshConfigWatchPaths();
}

void ValenzBridge::refreshConfigWatchPaths()
{
    if (!m_configWatcher)
        return;

    if (!m_configWatcher->directories().contains(m_userConfigDirPath))
        m_configWatcher->addPath(m_userConfigDirPath);

    if (QFileInfo::exists(m_userConfigPath) && !m_configWatcher->files().contains(m_userConfigPath))
        m_configWatcher->addPath(m_userConfigPath);
}

void ValenzBridge::scheduleConfigReload()
{
    if (m_configReloadTimer)
        m_configReloadTimer->start();
}

void ValenzBridge::reloadConfig()
{
    refreshConfigWatchPaths();
    if (!QFileInfo::exists(m_userConfigPath))
        return;

    QSettings settings(m_userConfigPath, QSettings::IniFormat);
    settings.sync();

    const auto update = [](auto &current, const auto &value, const auto &notify) {
        if (current == value)
            return false;
        current = value;
        notify();
        return true;
    };

    update(m_controlCenterIconMode,
           normalizeControlCenterIconMode(settings.value(kControlCenterIconModeKey, QStringLiteral("system16")).toString()),
           [this] { Q_EMIT controlCenterIconModeChanged(m_controlCenterIconMode); });
    update(m_controlCenterPowerCommand,
           normalizePowerCommand(settings.value(kControlCenterPowerCommandKey, QStringLiteral("wlogout")).toString()),
           [this] { Q_EMIT controlCenterPowerCommandChanged(m_controlCenterPowerCommand); });

    QString settingsCommand = settings.value(kControlCenterSettingsCommandKey, QStringLiteral("systemsettings")).toString().trimmed();
    if (settingsCommand.isEmpty())
        settingsCommand = QStringLiteral("systemsettings");
    update(m_controlCenterSettingsCommand, settingsCommand,
           [this] { Q_EMIT controlCenterSettingsCommandChanged(m_controlCenterSettingsCommand); });

    QString launcherCommand = settings.value(kLauncherCommandKey, QStringLiteral("vicinae toggle")).toString().trimmed();
    if (launcherCommand.isEmpty())
        launcherCommand = QStringLiteral("vicinae toggle");
    update(m_launcherCommand, launcherCommand,
           [this] { Q_EMIT launcherCommandChanged(m_launcherCommand); });

    QString clipboardCommand = settings.value(kClipboardCommandKey, QStringLiteral("vicinae vicinae://launch/clipboard/history")).toString().trimmed();
    if (clipboardCommand.isEmpty())
        clipboardCommand = QStringLiteral("vicinae vicinae://launch/clipboard/history");
    update(m_clipboardCommand, clipboardCommand,
           [this] { Q_EMIT clipboardCommandChanged(m_clipboardCommand); });

    QString diskUsagePath = settings.value(kControlCenterDiskUsagePathKey, QStringLiteral("/")).toString().trimmed();
    if (diskUsagePath.isEmpty())
        diskUsagePath = QStringLiteral("/");
    if (!diskUsagePath.startsWith(QLatin1Char('/')))
        diskUsagePath.prepend(QLatin1Char('/'));
    const bool diskUsagePathChanged = update(m_controlCenterDiskUsagePath, diskUsagePath,
                                             [this] { Q_EMIT controlCenterDiskUsagePathChanged(m_controlCenterDiskUsagePath); });

    update(m_mprisAlwaysVisible, settings.value(kMprisAlwaysVisibleKey, false).toBool(),
           [this] { Q_EMIT mprisAlwaysVisibleChanged(m_mprisAlwaysVisible); });
    update(m_barHeight, qBound(1, settings.value(kWindowBarHeightKey, 56).toInt(), kWindowBarHeightMax),
           [this] { Q_EMIT barHeightChanged(m_barHeight); });
    update(m_barLayerSpacing, qBound(0, settings.value(kWindowBarLayerSpacingKey, 0).toInt(), 64),
           [this] { Q_EMIT barLayerSpacingChanged(m_barLayerSpacing); });
    update(m_barLayerSpacingTop, qBound(0, settings.value(kWindowBarLayerSpacingTopKey, m_barLayerSpacing).toInt(), 64),
           [this] { Q_EMIT barLayerSpacingTopChanged(m_barLayerSpacingTop); });
    update(m_barLayerSpacingBottom, qBound(0, settings.value(kWindowBarLayerSpacingBottomKey, m_barLayerSpacing).toInt(), 64),
           [this] { Q_EMIT barLayerSpacingBottomChanged(m_barLayerSpacingBottom); });
    update(m_barLayerSpacingLeft, qBound(0, settings.value(kWindowBarLayerSpacingLeftKey, m_barLayerSpacing).toInt(), 64),
           [this] { Q_EMIT barLayerSpacingLeftChanged(m_barLayerSpacingLeft); });
    update(m_barLayerSpacingRight, qBound(0, settings.value(kWindowBarLayerSpacingRightKey, m_barLayerSpacing).toInt(), 64),
           [this] { Q_EMIT barLayerSpacingRightChanged(m_barLayerSpacingRight); });
    update(m_screenPlacement,
           normalizeScreenPlacement(settings.value(kWindowScreenPlacementKey, QStringLiteral("active")).toString()),
           [this] { Q_EMIT screenPlacementChanged(m_screenPlacement); });
    update(m_systemTrayDebugDetails, settings.value(kSystemTrayDebugDetailsKey, false).toBool(),
           [this] { Q_EMIT systemTrayDebugDetailsChanged(m_systemTrayDebugDetails); });

    bool controlCenterRuntimeChanged = false;
    controlCenterRuntimeChanged |= update(m_debugSimulatedBrightnessAvailable,
                                          settings.value(kDebugSimulatedBrightnessAvailableKey, false).toBool(), [] {});
    controlCenterRuntimeChanged |= update(m_debugSimulatedBrightnessPercentage,
                                          qBound(0, settings.value(kDebugSimulatedBrightnessPercentageKey, 65).toInt(), 100), [] {});
    controlCenterRuntimeChanged |= update(m_debugSimulatedBatteryAvailable,
                                          settings.value(kDebugSimulatedBatteryAvailableKey, false).toBool(), [] {});
    controlCenterRuntimeChanged |= update(m_debugSimulatedBatteryPercentage,
                                          qBound(0, settings.value(kDebugSimulatedBatteryPercentageKey, 72).toInt(), 100), [] {});
    controlCenterRuntimeChanged |= update(m_debugSimulatedBatteryCharging,
                                          settings.value(kDebugSimulatedBatteryChargingKey, false).toBool(), [] {});
    controlCenterRuntimeChanged |= update(m_debugSimulatedBatteryOnAcPower,
                                          settings.value(kDebugSimulatedBatteryOnAcPowerKey, false).toBool(), [] {});

    bool weatherChanged = false;
    weatherChanged |= update(m_weatherLatitude,
                             normalizeWeatherCoordinate(settings.value(kWeatherLatitudeKey, 40.7128), -90.0, 90.0, 40.7128),
                             [this] { Q_EMIT weatherLatitudeChanged(m_weatherLatitude); });
    weatherChanged |= update(m_weatherLongitude,
                             normalizeWeatherCoordinate(settings.value(kWeatherLongitudeKey, -74.0060), -180.0, 180.0, -74.0060),
                             [this] { Q_EMIT weatherLongitudeChanged(m_weatherLongitude); });
    weatherChanged |= update(m_weatherTemperatureUnit,
                             normalizeWeatherTemperatureUnit(settings.value(kWeatherTemperatureUnitKey, QStringLiteral("celsius")).toString()),
                             [this] { Q_EMIT weatherTemperatureUnitChanged(m_weatherTemperatureUnit); });
    const bool weatherIntervalChanged = update(m_weatherRefreshMinutes,
                                               normalizeWeatherRefreshMinutes(settings.value(kWeatherRefreshMinutesKey, 20)),
                                               [this] { Q_EMIT weatherRefreshMinutesChanged(m_weatherRefreshMinutes); });
    weatherChanged |= weatherIntervalChanged;

    if (weatherIntervalChanged)
        updateWeatherRefreshTimerInterval();
    if (weatherChanged)
        refreshWeather();
    if (diskUsagePathChanged)
        refreshControlCenterSystemResourcesState();
    if (controlCenterRuntimeChanged)
        refreshControlCenterRuntimeState();
    refreshSharedSettingsFromFile();
}

void ValenzBridge::persistControlCenterState() const
{
    if (m_userConfigPath.isEmpty())
        return;

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);
    userSettings.setValue(kControlCenterIconModeKey, m_controlCenterIconMode);
    userSettings.setValue(kControlCenterPowerCommandKey, m_controlCenterPowerCommand);
    userSettings.setValue(kControlCenterSettingsCommandKey, m_controlCenterSettingsCommand);
    userSettings.setValue(kLauncherCommandKey, m_launcherCommand);
    userSettings.setValue(kClipboardCommandKey, m_clipboardCommand);
    userSettings.setValue(kControlCenterDiskUsagePathKey, m_controlCenterDiskUsagePath);
    userSettings.setValue(kWindowScreenPlacementKey, m_screenPlacement);
    userSettings.sync();
}

void ValenzBridge::persistMprisState() const
{
    if (m_userConfigPath.isEmpty())
        return;

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);
    userSettings.setValue(kMprisAlwaysVisibleKey, m_mprisAlwaysVisible);
    userSettings.sync();
}

void ValenzBridge::persistWeatherState() const
{
    if (m_userConfigPath.isEmpty())
        return;

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);
    userSettings.setValue(kWeatherLatitudeKey, m_weatherLatitude);
    userSettings.setValue(kWeatherLongitudeKey, m_weatherLongitude);
    userSettings.setValue(kWeatherTemperatureUnitKey, m_weatherTemperatureUnit);
    userSettings.setValue(kWeatherRefreshMinutesKey, m_weatherRefreshMinutes);
    userSettings.sync();
}

void ValenzBridge::updateWeatherRefreshTimerInterval()
{
    if (!m_weatherRefreshTimer)
        return;

    const int normalizedMinutes = qBound(kWeatherRefreshMinMinutes,
                                         m_weatherRefreshMinutes,
                                         kWeatherRefreshMaxMinutes);
    m_weatherRefreshTimer->setInterval(normalizedMinutes * 60 * 1000);

    if (!m_weatherRefreshTimer->isActive())
        m_weatherRefreshTimer->start();
}

