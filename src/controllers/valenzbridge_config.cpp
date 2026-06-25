#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

#include <QDir>
#include <QFileInfo>
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
    ensureKey(QString::fromLatin1(kSystemTrayDebugDetailsKey), QString(), false);
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
    m_systemTrayDebugDetails = userSettings.value(kSystemTrayDebugDetailsKey, false).toBool();

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
    userSettings.setValue(kControlCenterPowerCommandKey, m_controlCenterPowerCommand);
    userSettings.setValue(kControlCenterSettingsCommandKey, m_controlCenterSettingsCommand);
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

