#include "valenzbridge.h"
#include "valenzbridge_p.h"

bool ValenzBridge::enabled() const
{
    return m_enabled;
}

void ValenzBridge::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    Q_EMIT enabledChanged(m_enabled);
}

int ValenzBridge::currentWorkspace() const
{
    return m_currentWorkspace;
}

void ValenzBridge::setCurrentWorkspace(int workspace)
{
    const int clampedWorkspace = clampWorkspace(workspace);
    if (m_currentWorkspace == clampedWorkspace)
        return;

    m_currentWorkspace = clampedWorkspace;
    Q_EMIT currentWorkspaceChanged(m_currentWorkspace);
}

int ValenzBridge::workspaceCount() const
{
    return m_workspaceCount;
}

void ValenzBridge::setWorkspaceCount(int count)
{
    const int normalizedCount = qMax(1, count);
    if (m_workspaceCount == normalizedCount)
        return;

    m_workspaceCount = normalizedCount;
    Q_EMIT workspaceCountChanged(m_workspaceCount);

    const int clampedCurrent = clampWorkspace(m_currentWorkspace);
    if (m_currentWorkspace != clampedCurrent)
    {
        m_currentWorkspace = clampedCurrent;
        Q_EMIT currentWorkspaceChanged(m_currentWorkspace);
    }
}

QString ValenzBridge::mediaTitle() const
{
    return m_mediaTitle;
}

void ValenzBridge::setMediaTitle(const QString &title)
{
    if (m_mediaTitle == title)
        return;

    m_mediaTitle = title;
    Q_EMIT mediaTitleChanged(m_mediaTitle);
}

QString ValenzBridge::mediaArtist() const
{
    return m_mediaArtist;
}

void ValenzBridge::setMediaArtist(const QString &artist)
{
    if (m_mediaArtist == artist)
        return;

    m_mediaArtist = artist;
    Q_EMIT mediaArtistChanged(m_mediaArtist);
}

QString ValenzBridge::mediaTimestamp() const
{
    return m_mediaTimestamp;
}

void ValenzBridge::setMediaTimestamp(const QString &timestamp)
{
    if (m_mediaTimestamp == timestamp)
        return;

    m_mediaTimestamp = timestamp;
    Q_EMIT mediaTimestampChanged(m_mediaTimestamp);
}

QString ValenzBridge::mediaArtSource() const
{
    return m_mediaArtSource;
}

void ValenzBridge::setMediaArtSource(const QString &source)
{
    if (m_mediaArtSource == source)
        return;

    m_mediaArtSource = source;
    Q_EMIT mediaArtSourceChanged(m_mediaArtSource);
}

bool ValenzBridge::mediaPlaying() const
{
    return m_mediaPlaying;
}

void ValenzBridge::setMediaPlaying(bool playing)
{
    if (m_mediaPlaying == playing)
        return;

    m_mediaPlaying = playing;
    Q_EMIT mediaPlayingChanged(m_mediaPlaying);
}

bool ValenzBridge::mprisVisible() const
{
    return m_mprisVisible;
}

void ValenzBridge::setMprisVisible(bool visible)
{
    if (m_mprisVisible == visible)
        return;

    m_mprisVisible = visible;
    Q_EMIT mprisVisibleChanged(m_mprisVisible);
}

bool ValenzBridge::mprisAlwaysVisible() const
{
    return m_mprisAlwaysVisible;
}

void ValenzBridge::setMprisAlwaysVisible(bool alwaysVisible)
{
    if (m_mprisAlwaysVisible == alwaysVisible)
        return;

    m_mprisAlwaysVisible = alwaysVisible;
    persistMprisState();
    Q_EMIT mprisAlwaysVisibleChanged(m_mprisAlwaysVisible);
}

bool ValenzBridge::agendaInstalled() const
{
    return m_agendaInstalled;
}

void ValenzBridge::setAgendaInstalled(bool installed)
{
    if (m_agendaInstalled == installed)
        return;

    m_agendaInstalled = installed;
    Q_EMIT agendaInstalledChanged(m_agendaInstalled);
}

QString ValenzBridge::weatherIconName() const
{
    return m_weatherIconName;
}

void ValenzBridge::setWeatherIconName(const QString &iconName)
{
    const QString trimmed = iconName.trimmed();
    if (m_weatherIconName == trimmed)
        return;

    m_weatherIconName = trimmed;
    Q_EMIT weatherIconNameChanged(m_weatherIconName);
}

QString ValenzBridge::weatherTemperature() const
{
    return m_weatherTemperature;
}

void ValenzBridge::setWeatherTemperature(const QString &temperature)
{
    const QString trimmed = temperature.trimmed();
    if (m_weatherTemperature == trimmed)
        return;

    m_weatherTemperature = trimmed;
    Q_EMIT weatherTemperatureChanged(m_weatherTemperature);
}

QString ValenzBridge::weatherConditionLabel() const
{
    return m_weatherConditionLabel;
}

void ValenzBridge::setWeatherConditionLabel(const QString &label)
{
    const QString trimmed = label.trimmed();
    if (m_weatherConditionLabel == trimmed)
        return;

    m_weatherConditionLabel = trimmed;
    Q_EMIT weatherConditionLabelChanged(m_weatherConditionLabel);
}

QString ValenzBridge::weatherLocationName() const
{
    return m_weatherLocationName;
}

void ValenzBridge::setWeatherLocationName(const QString &locationName)
{
    const QString trimmed = locationName.trimmed();
    if (m_weatherLocationName == trimmed)
        return;

    m_weatherLocationName = trimmed;
    Q_EMIT weatherLocationNameChanged(m_weatherLocationName);
}

double ValenzBridge::weatherLatitude() const
{
    return m_weatherLatitude;
}

void ValenzBridge::setWeatherLatitude(double latitude)
{
    const double bounded = qBound(-90.0, latitude, 90.0);
    if (qFuzzyCompare(m_weatherLatitude, bounded))
        return;

    m_weatherLatitude = bounded;
    persistWeatherState();
    Q_EMIT weatherLatitudeChanged(m_weatherLatitude);
    refreshWeather();
}

double ValenzBridge::weatherLongitude() const
{
    return m_weatherLongitude;
}

void ValenzBridge::setWeatherLongitude(double longitude)
{
    const double bounded = qBound(-180.0, longitude, 180.0);
    if (qFuzzyCompare(m_weatherLongitude, bounded))
        return;

    m_weatherLongitude = bounded;
    persistWeatherState();
    Q_EMIT weatherLongitudeChanged(m_weatherLongitude);
    refreshWeather();
}

QString ValenzBridge::weatherTemperatureUnit() const
{
    return m_weatherTemperatureUnit;
}

void ValenzBridge::setWeatherTemperatureUnit(const QString &unit)
{
    const QString normalized = normalizeWeatherTemperatureUnit(unit);
    if (m_weatherTemperatureUnit == normalized)
        return;

    m_weatherTemperatureUnit = normalized;
    persistWeatherState();
    Q_EMIT weatherTemperatureUnitChanged(m_weatherTemperatureUnit);
    refreshWeather();
}

int ValenzBridge::weatherRefreshMinutes() const
{
    return m_weatherRefreshMinutes;
}

void ValenzBridge::setWeatherRefreshMinutes(int minutes)
{
    const int normalized = qBound(kWeatherRefreshMinMinutes, minutes, kWeatherRefreshMaxMinutes);
    if (m_weatherRefreshMinutes == normalized)
        return;

    m_weatherRefreshMinutes = normalized;
    updateWeatherRefreshTimerInterval();
    persistWeatherState();
    Q_EMIT weatherRefreshMinutesChanged(m_weatherRefreshMinutes);
    refreshWeather();
}

QString ValenzBridge::focusedWindowTitle() const
{
    return m_focusedWindowTitle;
}

void ValenzBridge::setFocusedWindowTitle(const QString &title)
{
    if (m_focusedWindowTitle == title)
        return;

    m_focusedWindowTitle = title;
    Q_EMIT focusedWindowTitleChanged(m_focusedWindowTitle);
}

QString ValenzBridge::focusedWindowIconName() const
{
    return m_focusedWindowIconName;
}

void ValenzBridge::setFocusedWindowIconName(const QString &iconName)
{
    if (m_focusedWindowIconName == iconName)
        return;

    m_focusedWindowIconName = iconName;
    Q_EMIT focusedWindowIconNameChanged(m_focusedWindowIconName);
}

QString ValenzBridge::userRealName() const
{
    return m_userRealName;
}

QString ValenzBridge::controlCenterIconMode() const
{
    return m_controlCenterIconMode;
}

void ValenzBridge::setControlCenterIconMode(const QString &mode)
{
    if (m_controlCenterIconMode == mode)
        return;

    m_controlCenterIconMode = mode;
    persistControlCenterState();
    Q_EMIT controlCenterIconModeChanged(m_controlCenterIconMode);
}

QString ValenzBridge::prototypeNetworkState() const
{
    return m_prototypeNetworkState;
}

void ValenzBridge::setPrototypeNetworkState(const QString &state)
{
    const QString normalized = normalizePrototypeNetworkState(state);
    if (m_prototypeNetworkState == normalized)
        return;

    m_prototypeNetworkState = normalized;
    persistControlCenterState();
    Q_EMIT prototypeNetworkStateChanged(m_prototypeNetworkState);
}

QString ValenzBridge::prototypeBluetoothState() const
{
    return m_prototypeBluetoothState;
}

void ValenzBridge::setPrototypeBluetoothState(const QString &state)
{
    const QString normalized = normalizePrototypeBluetoothState(state);
    if (m_prototypeBluetoothState == normalized)
        return;

    m_prototypeBluetoothState = normalized;
    persistControlCenterState();
    Q_EMIT prototypeBluetoothStateChanged(m_prototypeBluetoothState);
}

QString ValenzBridge::prototypeVolumeState() const
{
    return m_prototypeVolumeState;
}

void ValenzBridge::setPrototypeVolumeState(const QString &state)
{
    const QString normalized = normalizePrototypeVolumeState(state);
    if (m_prototypeVolumeState == normalized)
        return;

    m_prototypeVolumeState = normalized;
    persistControlCenterState();
    Q_EMIT prototypeVolumeStateChanged(m_prototypeVolumeState);
}

QStringList ValenzBridge::controlCenterPowerProfiles() const
{
    return m_controlCenterPowerProfiles;
}

void ValenzBridge::setControlCenterPowerProfiles(const QStringList &profiles)
{
    const QStringList normalizedProfiles = normalizePowerProfiles(profiles);
    const QString normalizedCurrent = normalizeCurrentPowerProfile(m_controlCenterPowerProfileCurrent, normalizedProfiles);

    const bool profilesChanged = m_controlCenterPowerProfiles != normalizedProfiles;
    const bool currentChanged = m_controlCenterPowerProfileCurrent != normalizedCurrent;
    if (!profilesChanged && !currentChanged)
        return;

    m_controlCenterPowerProfiles = normalizedProfiles;
    m_controlCenterPowerProfileCurrent = normalizedCurrent;
    persistControlCenterState();

    if (profilesChanged)
        Q_EMIT controlCenterPowerProfilesChanged(m_controlCenterPowerProfiles);
    if (currentChanged)
        Q_EMIT controlCenterPowerProfileCurrentChanged(m_controlCenterPowerProfileCurrent);
}

QString ValenzBridge::controlCenterPowerProfileCurrent() const
{
    return m_controlCenterPowerProfileCurrent;
}

void ValenzBridge::setControlCenterPowerProfileCurrent(const QString &profile)
{
    const QString normalized = normalizeCurrentPowerProfile(profile, m_controlCenterPowerProfiles);
    if (m_controlCenterPowerProfileCurrent == normalized)
        return;

    m_controlCenterPowerProfileCurrent = normalized;
    persistControlCenterState();
    Q_EMIT controlCenterPowerProfileCurrentChanged(m_controlCenterPowerProfileCurrent);
}

QString ValenzBridge::controlCenterVolumePercentage() const
{
    return m_controlCenterVolumePercentage;
}

void ValenzBridge::setControlCenterVolumePercentage(const QString &value)
{
    const QString normalized = normalizeBatteryPercentage(value);
    if (m_controlCenterVolumePercentage == normalized)
        return;

    m_controlCenterVolumePercentage = normalized;
    Q_EMIT controlCenterVolumePercentageChanged(m_controlCenterVolumePercentage);
}

bool ValenzBridge::controlCenterBatteryCharging() const
{
    return m_controlCenterBatteryCharging;
}

void ValenzBridge::setControlCenterBatteryCharging(bool charging)
{
    if (m_controlCenterBatteryCharging == charging)
        return;

    m_controlCenterBatteryCharging = charging;
    Q_EMIT controlCenterBatteryChargingChanged(m_controlCenterBatteryCharging);
}

QString ValenzBridge::controlCenterBatteryPercentage() const
{
    return m_controlCenterBatteryPercentage;
}

void ValenzBridge::setControlCenterBatteryPercentage(const QString &value)
{
    const QString normalized = normalizeBatteryPercentage(value);
    if (m_controlCenterBatteryPercentage == normalized)
        return;

    m_controlCenterBatteryPercentage = normalized;
    Q_EMIT controlCenterBatteryPercentageChanged(m_controlCenterBatteryPercentage);
}

QString ValenzBridge::controlCenterNetworkState() const
{
    return m_controlCenterNetworkState;
}

void ValenzBridge::setControlCenterNetworkState(const QString &state)
{
    const QString normalized = normalizePrototypeNetworkState(state);
    if (m_controlCenterNetworkState == normalized)
        return;

    m_controlCenterNetworkState = normalized;
    Q_EMIT controlCenterNetworkStateChanged(m_controlCenterNetworkState);
}

bool ValenzBridge::controlCenterBluetoothEnabled() const
{
    return m_controlCenterBluetoothEnabled;
}

void ValenzBridge::setControlCenterBluetoothEnabled(bool enabled)
{
    if (m_controlCenterBluetoothEnabled == enabled)
        return;

    m_controlCenterBluetoothEnabled = enabled;
    Q_EMIT controlCenterBluetoothEnabledChanged(m_controlCenterBluetoothEnabled);
}

bool ValenzBridge::controlCenterBluetoothAvailable() const
{
    return m_controlCenterBluetoothAvailable;
}

void ValenzBridge::setControlCenterBluetoothAvailable(bool available)
{
    if (m_controlCenterBluetoothAvailable == available)
        return;

    m_controlCenterBluetoothAvailable = available;
    Q_EMIT controlCenterBluetoothAvailableChanged(m_controlCenterBluetoothAvailable);
}

bool ValenzBridge::controlCenterVolumeMuted() const
{
    return m_controlCenterVolumeMuted;
}

void ValenzBridge::setControlCenterVolumeMuted(bool muted)
{
    if (m_controlCenterVolumeMuted == muted)
        return;

    m_controlCenterVolumeMuted = muted;
    Q_EMIT controlCenterVolumeMutedChanged(m_controlCenterVolumeMuted);
}

bool ValenzBridge::controlCenterBatteryAvailable() const
{
    return m_controlCenterBatteryAvailable;
}

void ValenzBridge::setControlCenterBatteryAvailable(bool available)
{
    if (m_controlCenterBatteryAvailable == available)
        return;

    m_controlCenterBatteryAvailable = available;
    Q_EMIT controlCenterBatteryAvailableChanged(m_controlCenterBatteryAvailable);
}

bool ValenzBridge::controlCenterBatteryOnAcPower() const
{
    return m_controlCenterBatteryOnAcPower;
}

void ValenzBridge::setControlCenterBatteryOnAcPower(bool onAcPower)
{
    if (m_controlCenterBatteryOnAcPower == onAcPower)
        return;

    m_controlCenterBatteryOnAcPower = onAcPower;
    Q_EMIT controlCenterBatteryOnAcPowerChanged(m_controlCenterBatteryOnAcPower);
}

QString ValenzBridge::controlCenterPowerCommand() const
{
    return m_controlCenterPowerCommand;
}

void ValenzBridge::setControlCenterPowerCommand(const QString &command)
{
    const QString normalized = normalizePowerCommand(command);
    if (m_controlCenterPowerCommand == normalized)
        return;

    m_controlCenterPowerCommand = normalized;
    persistControlCenterState();
    Q_EMIT controlCenterPowerCommandChanged(m_controlCenterPowerCommand);
}

void ValenzBridge::trace(const QString &source, const QString &action, const QString &detail)
{
    if (!m_enabled)
        return;

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    Q_EMIT traceRaised(source, action, detail, timestamp);
}

