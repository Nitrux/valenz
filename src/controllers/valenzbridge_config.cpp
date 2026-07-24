#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QTimer>
#include <QUrl>

QString ValenzBridge::configFilePath() const
{
    return m_userConfigPath;
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
    if (m_controlCenterSettingsCommand.isEmpty())
        m_controlCenterSettingsCommand = QStringLiteral("systemsettings");
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
}

void ValenzBridge::persistControlCenterState() const
{
    if (m_userConfigPath.isEmpty())
        return;

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);
    userSettings.setValue(kControlCenterIconModeKey, m_controlCenterIconMode);
    userSettings.setValue(kControlCenterPowerCommandKey, m_controlCenterPowerCommand);
    userSettings.setValue(kControlCenterSettingsCommandKey, m_controlCenterSettingsCommand);
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

