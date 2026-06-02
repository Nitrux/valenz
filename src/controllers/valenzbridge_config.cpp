#include "valenzbridge.h"
#include "valenzbridge_p.h"

QString ValenzBridge::configFilePath() const
{
    return m_userConfigPath;
}

int ValenzBridge::clampWorkspace(int workspace) const
{
    return qBound(1, workspace, m_workspaceCount);
}

void ValenzBridge::initializeConfig()
{
    const QString configDir = QDir::home().filePath(".config/valenz");
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
    ensureKey(QString::fromLatin1(kFocusedWindowIconNameKey), QString::fromLatin1(kLegacyFocusedWindowIconNameKey), "application-x-executable");
    ensureKey(QString::fromLatin1(kControlCenterIconModeKey), QString::fromLatin1(kLegacyControlCenterIconModeKey), "auto");
    ensureKey(QString::fromLatin1(kControlCenterPowerProfilesKey), QString::fromLatin1(kLegacyControlCenterPowerProfilesKey),
              QStringList { QStringLiteral("power-saver"), QStringLiteral("balanced"), QStringLiteral("performance") });
    ensureKey(QString::fromLatin1(kControlCenterPowerProfileCurrentKey), QString::fromLatin1(kLegacyControlCenterPowerProfileCurrentKey), "balanced");
    ensureKey(QString::fromLatin1(kControlCenterVolumePercentageKey), QString::fromLatin1(kLegacyControlCenterVolumePercentageKey), "50%");

    if (!userSettings.contains(kControlCenterBatteryStateKey))
    {
        if (userSettings.contains(kLegacyControlCenterBatteryStateKey))
        {
            userSettings.setValue(kControlCenterBatteryStateKey, userSettings.value(kLegacyControlCenterBatteryStateKey));
            userSettings.remove(kLegacyControlCenterBatteryStateKey);
        }
        else
        {
            const QVariant legacyBatteryIcon = userSettings.contains("ControlCenter/batteryIconName")
                                                   ? userSettings.value("ControlCenter/batteryIconName")
                                                   : userSettings.value("controlCenter/batteryIconName");
            userSettings.setValue(kControlCenterBatteryStateKey,
                                  normalizeControlCenterBatteryCharging(legacyBatteryIcon) ? "charging" : "battery");
        }
    }

    ensureKey(QString::fromLatin1(kControlCenterBatteryPercentageKey), QString::fromLatin1(kLegacyControlCenterBatteryPercentageKey), "0%");
    ensureKey(QString::fromLatin1(kControlCenterPowerCommandKey), QString::fromLatin1(kLegacyControlCenterPowerCommandKey), "wlogout");
    ensureKey(QString::fromLatin1(kWeatherLatitudeKey), QString(), 40.7128);
    ensureKey(QString::fromLatin1(kWeatherLongitudeKey), QString(), -74.0060);
    ensureKey(QString::fromLatin1(kWeatherTemperatureUnitKey), QString(), "celsius");
    ensureKey(QString::fromLatin1(kWeatherRefreshMinutesKey), QString(), 20);
    ensureKey(QString::fromLatin1(kMprisAlwaysVisibleKey), QString(), false);
    ensureKey(QString::fromLatin1(kControlCenterDiskUsagePathKey), QString::fromLatin1(kLegacyControlCenterDiskUsagePathKey), "/");

    userSettings.remove("ControlCenter/batteryIconName");
    userSettings.remove("controlCenter/batteryIconName");
    userSettings.remove("ControlCenter/powerProfileIconName");
    userSettings.remove("Window/focusedWindowTitle");
    userSettings.remove("window/title");

    userSettings.sync();
    m_focusedWindowTitle.clear();
    m_focusedWindowIconName = userSettings.value(kFocusedWindowIconNameKey, "application-x-executable").toString();
    m_userRealName = systemUserRealName();
    m_controlCenterIconMode = userSettings.value(kControlCenterIconModeKey, "auto").toString();
    m_controlCenterNetworkMode = QStringLiteral("auto");
    m_controlCenterBluetoothState = QStringLiteral("auto");
    m_controlCenterVolumeState = QStringLiteral("auto");
    m_controlCenterPowerProfiles = normalizePowerProfiles(userSettings.value(kControlCenterPowerProfilesKey,
                                                                             QStringList { QStringLiteral("power-saver"),
                                                                                           QStringLiteral("balanced"),
                                                                                           QStringLiteral("performance") }));
    m_controlCenterPowerProfileCurrent = normalizeCurrentPowerProfile(userSettings.value(kControlCenterPowerProfileCurrentKey, "balanced").toString(),
                                                                      m_controlCenterPowerProfiles);
    m_controlCenterVolumePercentage = normalizeBatteryPercentage(userSettings.value(kControlCenterVolumePercentageKey, "50%").toString());
    m_controlCenterBatteryCharging = normalizeControlCenterBatteryCharging(userSettings.value(kControlCenterBatteryStateKey, "battery"));
    m_controlCenterBatteryPercentage = normalizeBatteryPercentage(userSettings.value(kControlCenterBatteryPercentageKey, "0%").toString());
    m_controlCenterPowerCommand = normalizePowerCommand(userSettings.value(kControlCenterPowerCommandKey, "wlogout").toString());
    m_controlCenterDiskUsagePath = userSettings.value(kControlCenterDiskUsagePathKey, "/").toString().trimmed();
    if (m_controlCenterDiskUsagePath.isEmpty())
        m_controlCenterDiskUsagePath = QStringLiteral("/");
    if (!m_controlCenterDiskUsagePath.startsWith(QLatin1Char('/')))
        m_controlCenterDiskUsagePath.prepend(QLatin1Char('/'));
    m_mprisAlwaysVisible = userSettings.value(kMprisAlwaysVisibleKey, false).toBool();


    m_weatherLatitude = normalizeWeatherCoordinate(userSettings.value(kWeatherLatitudeKey, 40.7128), -90.0, 90.0, 40.7128);
    m_weatherLongitude = normalizeWeatherCoordinate(userSettings.value(kWeatherLongitudeKey, -74.0060), -180.0, 180.0, -74.0060);
    m_weatherTemperatureUnit = normalizeWeatherTemperatureUnit(userSettings.value(kWeatherTemperatureUnitKey, "celsius").toString());
    m_weatherRefreshMinutes = normalizeWeatherRefreshMinutes(userSettings.value(kWeatherRefreshMinutesKey, 20));

    m_weatherIconName = QStringLiteral("weather-severe-alert");
    m_weatherTemperature = m_weatherTemperatureUnit == QLatin1String("fahrenheit") ? QStringLiteral("--°F") : QStringLiteral("--°C");
    m_weatherConditionLabel = QString();
    m_weatherLocationName = QString();
}

void ValenzBridge::persistControlCenterState() const
{
    if (m_userConfigPath.isEmpty())
        return;

    QSettings userSettings(m_userConfigPath, QSettings::IniFormat);
    userSettings.setValue(kControlCenterIconModeKey, m_controlCenterIconMode);
    userSettings.setValue(kControlCenterPowerProfilesKey, m_controlCenterPowerProfiles);
    userSettings.setValue(kControlCenterPowerProfileCurrentKey, m_controlCenterPowerProfileCurrent);
    userSettings.setValue(kControlCenterVolumePercentageKey, m_controlCenterVolumePercentage);
    userSettings.setValue(kControlCenterBatteryStateKey, m_controlCenterBatteryCharging ? "charging" : "battery");
    userSettings.setValue(kControlCenterBatteryPercentageKey, m_controlCenterBatteryPercentage);
    userSettings.setValue(kControlCenterPowerCommandKey, m_controlCenterPowerCommand);
    userSettings.setValue(kControlCenterDiskUsagePathKey, m_controlCenterDiskUsagePath);
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

