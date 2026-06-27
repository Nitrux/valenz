#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVariantList>
#include <QVariantMap>

class QLocalSocket;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class ValenzBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(int currentWorkspace READ currentWorkspace WRITE setCurrentWorkspace NOTIFY currentWorkspaceChanged FINAL)
    Q_PROPERTY(int workspaceCount READ workspaceCount WRITE setWorkspaceCount NOTIFY workspaceCountChanged FINAL)
    Q_PROPERTY(QString mediaTitle READ mediaTitle WRITE setMediaTitle NOTIFY mediaTitleChanged FINAL)
    Q_PROPERTY(QString mediaArtist READ mediaArtist WRITE setMediaArtist NOTIFY mediaArtistChanged FINAL)
    Q_PROPERTY(QString mediaTimestamp READ mediaTimestamp WRITE setMediaTimestamp NOTIFY mediaTimestampChanged FINAL)
    Q_PROPERTY(QString mediaArtSource READ mediaArtSource WRITE setMediaArtSource NOTIFY mediaArtSourceChanged FINAL)
    Q_PROPERTY(bool mediaPlaying READ mediaPlaying WRITE setMediaPlaying NOTIFY mediaPlayingChanged FINAL)
    Q_PROPERTY(bool mprisVisible READ mprisVisible WRITE setMprisVisible NOTIFY mprisVisibleChanged FINAL)
    Q_PROPERTY(bool mprisAlwaysVisible READ mprisAlwaysVisible WRITE setMprisAlwaysVisible NOTIFY mprisAlwaysVisibleChanged FINAL)
    Q_PROPERTY(QVariantList mprisSources READ mprisSources NOTIFY mprisSourcesChanged FINAL)
    Q_PROPERTY(QString focusedWindowTitle READ focusedWindowTitle WRITE setFocusedWindowTitle NOTIFY focusedWindowTitleChanged FINAL)
    Q_PROPERTY(QString focusedWindowIconName READ focusedWindowIconName WRITE setFocusedWindowIconName NOTIFY focusedWindowIconNameChanged FINAL)
    Q_PROPERTY(int focusedWindowFullscreenInternal READ focusedWindowFullscreenInternal NOTIFY focusedWindowFullscreenInternalChanged FINAL)
    Q_PROPERTY(int focusedWindowFullscreenClient READ focusedWindowFullscreenClient NOTIFY focusedWindowFullscreenClientChanged FINAL)
    Q_PROPERTY(bool focusedWindowFullscreen READ focusedWindowFullscreen NOTIFY focusedWindowFullscreenChanged FINAL)
    Q_PROPERTY(QString userRealName READ userRealName CONSTANT)
    Q_PROPERTY(QString userAvatarUrl READ userAvatarUrl CONSTANT)
    Q_PROPERTY(QString controlCenterIconMode READ controlCenterIconMode WRITE setControlCenterIconMode NOTIFY controlCenterIconModeChanged FINAL)
    Q_PROPERTY(QString controlCenterNetworkMode READ controlCenterNetworkMode WRITE setControlCenterNetworkMode NOTIFY controlCenterNetworkModeChanged FINAL)
    Q_PROPERTY(QString controlCenterBluetoothState READ controlCenterBluetoothState WRITE setControlCenterBluetoothState NOTIFY controlCenterBluetoothStateChanged FINAL)
    Q_PROPERTY(QString controlCenterVolumeState READ controlCenterVolumeState WRITE setControlCenterVolumeState NOTIFY controlCenterVolumeStateChanged FINAL)
    Q_PROPERTY(QStringList controlCenterPowerProfiles READ controlCenterPowerProfiles WRITE setControlCenterPowerProfiles NOTIFY controlCenterPowerProfilesChanged FINAL)
    Q_PROPERTY(QString controlCenterPowerProfileCurrent READ controlCenterPowerProfileCurrent WRITE setControlCenterPowerProfileCurrent NOTIFY controlCenterPowerProfileCurrentChanged FINAL)
    Q_PROPERTY(QString controlCenterVolumePercentage READ controlCenterVolumePercentage WRITE setControlCenterVolumePercentage NOTIFY controlCenterVolumePercentageChanged FINAL)
    Q_PROPERTY(bool controlCenterBatteryCharging READ controlCenterBatteryCharging WRITE setControlCenterBatteryCharging NOTIFY controlCenterBatteryChargingChanged FINAL)
    Q_PROPERTY(QString controlCenterBatteryPercentage READ controlCenterBatteryPercentage WRITE setControlCenterBatteryPercentage NOTIFY controlCenterBatteryPercentageChanged FINAL)
    Q_PROPERTY(int controlCenterCpuPercentage READ controlCenterCpuPercentage WRITE setControlCenterCpuPercentage NOTIFY controlCenterCpuPercentageChanged FINAL)
    Q_PROPERTY(int controlCenterRamPercentage READ controlCenterRamPercentage WRITE setControlCenterRamPercentage NOTIFY controlCenterRamPercentageChanged FINAL)
    Q_PROPERTY(int controlCenterDiskUsagePercentage READ controlCenterDiskUsagePercentage WRITE setControlCenterDiskUsagePercentage NOTIFY controlCenterDiskUsagePercentageChanged FINAL)
    Q_PROPERTY(QString controlCenterDiskUsageText READ controlCenterDiskUsageText WRITE setControlCenterDiskUsageText NOTIFY controlCenterDiskUsageTextChanged FINAL)
    Q_PROPERTY(QString controlCenterBrightnessPercentage READ controlCenterBrightnessPercentage WRITE setControlCenterBrightnessPercentage NOTIFY controlCenterBrightnessPercentageChanged FINAL)
    Q_PROPERTY(bool controlCenterBrightnessAvailable READ controlCenterBrightnessAvailable WRITE setControlCenterBrightnessAvailable NOTIFY controlCenterBrightnessAvailableChanged FINAL)
    Q_PROPERTY(QString controlCenterNetworkState READ controlCenterNetworkState WRITE setControlCenterNetworkState NOTIFY controlCenterNetworkStateChanged FINAL)
    Q_PROPERTY(bool controlCenterBluetoothEnabled READ controlCenterBluetoothEnabled WRITE setControlCenterBluetoothEnabled NOTIFY controlCenterBluetoothEnabledChanged FINAL)
    Q_PROPERTY(bool controlCenterBluetoothAvailable READ controlCenterBluetoothAvailable WRITE setControlCenterBluetoothAvailable NOTIFY controlCenterBluetoothAvailableChanged FINAL)
    Q_PROPERTY(bool controlCenterWirelessAvailable READ controlCenterWirelessAvailable WRITE setControlCenterWirelessAvailable NOTIFY controlCenterWirelessAvailableChanged FINAL)
    Q_PROPERTY(int controlCenterBluetoothConnectedDeviceCount READ controlCenterBluetoothConnectedDeviceCount WRITE setControlCenterBluetoothConnectedDeviceCount NOTIFY controlCenterBluetoothConnectedDeviceCountChanged FINAL)
    Q_PROPERTY(bool controlCenterVolumeMuted READ controlCenterVolumeMuted WRITE setControlCenterVolumeMuted NOTIFY controlCenterVolumeMutedChanged FINAL)
    Q_PROPERTY(bool controlCenterBatteryAvailable READ controlCenterBatteryAvailable WRITE setControlCenterBatteryAvailable NOTIFY controlCenterBatteryAvailableChanged FINAL)
    Q_PROPERTY(bool controlCenterBatteryOnAcPower READ controlCenterBatteryOnAcPower WRITE setControlCenterBatteryOnAcPower NOTIFY controlCenterBatteryOnAcPowerChanged FINAL)
    Q_PROPERTY(bool controlCenterNightLightEnabled READ controlCenterNightLightEnabled WRITE setControlCenterNightLightEnabled NOTIFY controlCenterNightLightEnabledChanged FINAL)
    Q_PROPERTY(bool controlCenterNightLightAvailable READ controlCenterNightLightAvailable WRITE setControlCenterNightLightAvailable NOTIFY controlCenterNightLightAvailableChanged FINAL)
    Q_PROPERTY(QString controlCenterPowerCommand READ controlCenterPowerCommand WRITE setControlCenterPowerCommand NOTIFY controlCenterPowerCommandChanged FINAL)
    Q_PROPERTY(QString controlCenterSettingsCommand READ controlCenterSettingsCommand WRITE setControlCenterSettingsCommand NOTIFY controlCenterSettingsCommandChanged FINAL)
    Q_PROPERTY(QString controlCenterDiskUsagePath READ controlCenterDiskUsagePath WRITE setControlCenterDiskUsagePath NOTIFY controlCenterDiskUsagePathChanged FINAL)
    Q_PROPERTY(int barHeight READ barHeight CONSTANT FINAL)
    Q_PROPERTY(int barLayerSpacing READ barLayerSpacing CONSTANT FINAL)
    Q_PROPERTY(int barLayerSpacingTop READ barLayerSpacingTop CONSTANT FINAL)
    Q_PROPERTY(int barLayerSpacingBottom READ barLayerSpacingBottom CONSTANT FINAL)
    Q_PROPERTY(int barLayerSpacingLeft READ barLayerSpacingLeft CONSTANT FINAL)
    Q_PROPERTY(int barLayerSpacingRight READ barLayerSpacingRight CONSTANT FINAL)
    Q_PROPERTY(bool systemTrayDebugDetails READ systemTrayDebugDetails CONSTANT FINAL)
    Q_PROPERTY(bool agendaInstalled READ agendaInstalled NOTIFY agendaInstalledChanged FINAL)
    Q_PROPERTY(QString weatherIconName READ weatherIconName WRITE setWeatherIconName NOTIFY weatherIconNameChanged FINAL)
    Q_PROPERTY(QString weatherTemperature READ weatherTemperature WRITE setWeatherTemperature NOTIFY weatherTemperatureChanged FINAL)
    Q_PROPERTY(QString weatherConditionLabel READ weatherConditionLabel WRITE setWeatherConditionLabel NOTIFY weatherConditionLabelChanged FINAL)
    Q_PROPERTY(QString weatherLocationName READ weatherLocationName NOTIFY weatherLocationNameChanged FINAL)
    Q_PROPERTY(double weatherLatitude READ weatherLatitude WRITE setWeatherLatitude NOTIFY weatherLatitudeChanged FINAL)
    Q_PROPERTY(double weatherLongitude READ weatherLongitude WRITE setWeatherLongitude NOTIFY weatherLongitudeChanged FINAL)
    Q_PROPERTY(QString weatherTemperatureUnit READ weatherTemperatureUnit WRITE setWeatherTemperatureUnit NOTIFY weatherTemperatureUnitChanged FINAL)
    Q_PROPERTY(int weatherRefreshMinutes READ weatherRefreshMinutes WRITE setWeatherRefreshMinutes NOTIFY weatherRefreshMinutesChanged FINAL)

public:
    explicit ValenzBridge(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);
    int currentWorkspace() const;
    void setCurrentWorkspace(int workspace);
    int workspaceCount() const;
    void setWorkspaceCount(int count);
    QString mediaTitle() const;
    void setMediaTitle(const QString &title);
    QString mediaArtist() const;
    void setMediaArtist(const QString &artist);
    QString mediaTimestamp() const;
    void setMediaTimestamp(const QString &timestamp);
    QString mediaArtSource() const;
    void setMediaArtSource(const QString &source);
    bool mediaPlaying() const;
    void setMediaPlaying(bool playing);
    bool mprisVisible() const;
    void setMprisVisible(bool visible);
    bool mprisAlwaysVisible() const;
    void setMprisAlwaysVisible(bool alwaysVisible);
    QVariantList mprisSources() const;
    QString focusedWindowTitle() const;
    void setFocusedWindowTitle(const QString &title);
    QString focusedWindowIconName() const;
    void setFocusedWindowIconName(const QString &iconName);
    int focusedWindowFullscreenInternal() const;
    void setFocusedWindowFullscreenInternal(int fullscreen);
    int focusedWindowFullscreenClient() const;
    void setFocusedWindowFullscreenClient(int fullscreen);
    bool focusedWindowFullscreen() const;
    QString userRealName() const;
    QString userAvatarUrl() const;
    QString controlCenterIconMode() const;
    void setControlCenterIconMode(const QString &mode);
    QString controlCenterNetworkMode() const;
    void setControlCenterNetworkMode(const QString &state);
    QString controlCenterBluetoothState() const;
    void setControlCenterBluetoothState(const QString &state);
    QString controlCenterVolumeState() const;
    void setControlCenterVolumeState(const QString &state);
    QStringList controlCenterPowerProfiles() const;
    void setControlCenterPowerProfiles(const QStringList &profiles);
    QString controlCenterPowerProfileCurrent() const;
    void setControlCenterPowerProfileCurrent(const QString &profile);
    QString controlCenterVolumePercentage() const;
    void setControlCenterVolumePercentage(const QString &value);
    bool controlCenterBatteryCharging() const;
    void setControlCenterBatteryCharging(bool charging);
    QString controlCenterBatteryPercentage() const;
    void setControlCenterBatteryPercentage(const QString &value);
    int controlCenterCpuPercentage() const;
    void setControlCenterCpuPercentage(int percent);
    int controlCenterRamPercentage() const;
    void setControlCenterRamPercentage(int percent);
    int controlCenterDiskUsagePercentage() const;
    void setControlCenterDiskUsagePercentage(int percent);
    QString controlCenterDiskUsageText() const;
    void setControlCenterDiskUsageText(const QString &text);
    QString controlCenterBrightnessPercentage() const;
    void setControlCenterBrightnessPercentage(const QString &value);
    bool controlCenterBrightnessAvailable() const;
    void setControlCenterBrightnessAvailable(bool available);
    QString controlCenterNetworkState() const;
    void setControlCenterNetworkState(const QString &state);
    bool controlCenterBluetoothEnabled() const;
    void setControlCenterBluetoothEnabled(bool enabled);
    bool controlCenterBluetoothAvailable() const;
    void setControlCenterBluetoothAvailable(bool available);
    bool controlCenterWirelessAvailable() const;
    void setControlCenterWirelessAvailable(bool available);
    int controlCenterBluetoothConnectedDeviceCount() const;
    void setControlCenterBluetoothConnectedDeviceCount(int count);
    bool controlCenterVolumeMuted() const;
    void setControlCenterVolumeMuted(bool muted);
    bool controlCenterBatteryAvailable() const;
    void setControlCenterBatteryAvailable(bool available);
    bool controlCenterBatteryOnAcPower() const;
    void setControlCenterBatteryOnAcPower(bool onAcPower);
    bool controlCenterNightLightEnabled() const;
    void setControlCenterNightLightEnabled(bool enabled);
    bool controlCenterNightLightAvailable() const;
    void setControlCenterNightLightAvailable(bool available);
    QString controlCenterPowerCommand() const;
    void setControlCenterPowerCommand(const QString &command);
    QString controlCenterSettingsCommand() const;
    void setControlCenterSettingsCommand(const QString &command);
    QString controlCenterDiskUsagePath() const;
    void setControlCenterDiskUsagePath(const QString &path);
    int barHeight() const;
    int barLayerSpacing() const;
    int barLayerSpacingTop() const;
    int barLayerSpacingBottom() const;
    int barLayerSpacingLeft() const;
    int barLayerSpacingRight() const;
    bool systemTrayDebugDetails() const;
    bool agendaInstalled() const;
    QString weatherIconName() const;
    void setWeatherIconName(const QString &iconName);
    QString weatherTemperature() const;
    void setWeatherTemperature(const QString &temperature);
    QString weatherConditionLabel() const;
    void setWeatherConditionLabel(const QString &label);
    QString weatherLocationName() const;
    double weatherLatitude() const;
    void setWeatherLatitude(double latitude);
    double weatherLongitude() const;
    void setWeatherLongitude(double longitude);
    QString weatherTemperatureUnit() const;
    void setWeatherTemperatureUnit(const QString &unit);
    int weatherRefreshMinutes() const;
    void setWeatherRefreshMinutes(int minutes);

    Q_INVOKABLE void trace(const QString &source, const QString &action, const QString &detail = QString());
    Q_INVOKABLE void executeControlCenterPowerCommand();
    Q_INVOKABLE void executeControlCenterSettingsCommand();
    Q_INVOKABLE void setControlCenterVolumePercentageFromSlider(int percent);
    Q_INVOKABLE void setControlCenterBrightnessPercentageFromSlider(int percent);
    Q_INVOKABLE void setControlCenterRuntimeActive(bool active);
    Q_INVOKABLE void goToPreviousWorkspace();
    Q_INVOKABLE void goToNextWorkspace();
    Q_INVOKABLE bool refreshWorkspaceState();
    Q_INVOKABLE void mediaPreviousTrack();
    Q_INVOKABLE void mediaTogglePlayPause();
    Q_INVOKABLE void mediaNextTrack();
    Q_INVOKABLE void selectMprisSource(const QString &serviceName);
    Q_INVOKABLE QString configFilePath() const;
    Q_INVOKABLE QVariantList controlCenterDiskUsageOptions() const;
    Q_INVOKABLE void refreshControlCenterSystemResources();
    Q_INVOKABLE void refreshControlCenterRuntimeState();
    Q_INVOKABLE void refreshWeather();

Q_SIGNALS:
    void traceRaised(const QString &source, const QString &action, const QString &detail, const QString &timestamp);
    void enabledChanged(bool enabled);
    void currentWorkspaceChanged(int currentWorkspace);
    void workspaceCountChanged(int workspaceCount);
    void mediaTitleChanged(const QString &title);
    void mediaArtistChanged(const QString &artist);
    void mediaTimestampChanged(const QString &timestamp);
    void mediaArtSourceChanged(const QString &source);
    void mediaPlayingChanged(bool playing);
    void mprisVisibleChanged(bool visible);
    void mprisAlwaysVisibleChanged(bool alwaysVisible);
    void mprisSourcesChanged();
    void focusedWindowTitleChanged(const QString &title);
    void focusedWindowIconNameChanged(const QString &iconName);
    void focusedWindowFullscreenInternalChanged(int fullscreen);
    void focusedWindowFullscreenClientChanged(int fullscreen);
    void focusedWindowFullscreenChanged(bool fullscreen);
    void controlCenterPowerCommandChanged(const QString &command);
    void controlCenterSettingsCommandChanged(const QString &command);
    void controlCenterDiskUsagePathChanged(const QString &path);
    void controlCenterIconModeChanged(const QString &mode);
    void controlCenterNetworkModeChanged(const QString &state);
    void controlCenterBluetoothStateChanged(const QString &state);
    void controlCenterVolumeStateChanged(const QString &state);
    void controlCenterPowerProfilesChanged(const QStringList &profiles);
    void controlCenterPowerProfileCurrentChanged(const QString &profile);
    void controlCenterVolumePercentageChanged(const QString &value);
    void controlCenterBatteryChargingChanged(bool charging);
    void controlCenterBatteryPercentageChanged(const QString &value);
    void controlCenterCpuPercentageChanged(int percent);
    void controlCenterRamPercentageChanged(int percent);
    void controlCenterDiskUsagePercentageChanged(int percent);
    void controlCenterDiskUsageTextChanged(const QString &text);
    void controlCenterBrightnessPercentageChanged(const QString &value);
    void controlCenterBrightnessAvailableChanged(bool available);
    void controlCenterNetworkStateChanged(const QString &state);
    void controlCenterBluetoothEnabledChanged(bool enabled);
    void controlCenterBluetoothAvailableChanged(bool available);
    void controlCenterWirelessAvailableChanged(bool available);
    void controlCenterBluetoothConnectedDeviceCountChanged(int count);
    void controlCenterVolumeMutedChanged(bool muted);
    void controlCenterBatteryAvailableChanged(bool available);
    void controlCenterBatteryOnAcPowerChanged(bool onAcPower);
    void controlCenterNightLightEnabledChanged(bool enabled);
    void controlCenterNightLightAvailableChanged(bool available);
    void agendaInstalledChanged(bool installed);
    void weatherIconNameChanged(const QString &iconName);
    void weatherTemperatureChanged(const QString &temperature);
    void weatherConditionLabelChanged(const QString &label);
    void weatherLocationNameChanged(const QString &locationName);
    void weatherLatitudeChanged(double latitude);
    void weatherLongitudeChanged(double longitude);
    void weatherTemperatureUnitChanged(const QString &unit);
    void weatherRefreshMinutesChanged(int minutes);

private Q_SLOTS:
    void onMprisPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);
    void onMprisNameOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    void onWeatherReplyFinished(QNetworkReply *reply);

private:
    int clampWorkspace(int workspace) const;
    void initializeConfig();
    void connectHyprlandEventSocket();
    void scheduleHyprlandEventSocketReconnect();
    void handleHyprlandEventData();
    void handleHyprlandEventLine(const QString &line);
    void scheduleWorkspaceStateRefresh(int delayMs = 0);
    void scheduleFocusedWindowStateRefresh(int delayMs = 0);
    bool refreshFocusedWindowState();
    QStringList mprisServiceNames() const;
    QString preferredMprisService() const;
    QVariantMap mprisPlayerProperties(const QString &serviceName) const;
    QVariantMap mprisPlayerMetadata(const QString &serviceName) const;
    qint64 mprisPlayerPositionUs(const QString &serviceName) const;
    QString formatMprisTimeUs(qint64 microseconds) const;
    QString formatMprisTimestamp(qint64 positionUs, qint64 lengthUs) const;
    void refreshMprisState();
    void updateMprisRefreshTimer();
    void clearMprisState();
    void setMprisSources(const QVariantList &sources);
    void updateMprisPlaybackTicker();
    void updateMprisTimestampFromTicker();
    void connectMprisSignalObservers();
    void updateMprisPropertiesSubscription(const QString &serviceName);
    void clearMprisPropertiesSubscription();
    bool invokeMprisPlayerMethod(const QString &method);
    void persistControlCenterState() const;
    void persistMprisState() const;
    void persistWeatherState() const;
    void updateWeatherRefreshTimerInterval();
    void setAgendaInstalled(bool installed);
    void setWeatherLocationName(const QString &locationName);
    void initializeControlCenterRuntime();
    void updateControlCenterRuntimeTimer();
    void refreshControlCenterNetworkState();
    void refreshControlCenterBluetoothState();
    void refreshControlCenterVolumeState();
    void refreshControlCenterSystemResourcesState();
    void refreshControlCenterBatteryState();
    void refreshControlCenterBrightnessState();
    void refreshControlCenterNightLightState();
    void refreshControlCenterPowerProfileState();

    bool m_enabled = true;
    int m_currentWorkspace = 1;
    int m_workspaceCount = 1;
    QString m_mediaTitle;
    QString m_mediaArtist;
    QString m_mediaTimestamp;
    QString m_mediaArtSource;
    bool m_mediaPlaying = false;
    bool m_mprisVisible = false;
    bool m_mprisAlwaysVisible = false;
    bool m_systemTrayDebugDetails = false;
    QVariantList m_mprisSources;
    QString m_focusedWindowTitle;
    QString m_focusedWindowIconName;
    int m_focusedWindowFullscreenInternal = 0;
    int m_focusedWindowFullscreenClient = 0;
    QString m_userRealName;
    QString m_userAvatarPath;
    QString m_controlCenterIconMode;
    QString m_controlCenterNetworkMode;
    QString m_controlCenterBluetoothState;
    QString m_controlCenterVolumeState;
    QStringList m_controlCenterPowerProfiles;
    QString m_controlCenterPowerProfileCurrent;
    QString m_controlCenterVolumePercentage;
    bool m_controlCenterVolumeMuted = false;
    bool m_controlCenterBatteryCharging = false;
    QString m_controlCenterBatteryPercentage;
    int m_controlCenterCpuPercentage = 0;
    int m_controlCenterRamPercentage = 0;
    int m_controlCenterDiskUsagePercentage = 0;
    QString m_controlCenterDiskUsageText;
    QString m_controlCenterBrightnessPercentage = QStringLiteral("0%");
    bool m_controlCenterBrightnessAvailable = false;
    bool m_debugSimulatedBrightnessAvailable = false;
    int m_debugSimulatedBrightnessPercentage = 65;
    QString m_controlCenterNetworkState = QStringLiteral("offline");
    bool m_controlCenterBluetoothEnabled = false;
    bool m_controlCenterBluetoothAvailable = false;
    bool m_controlCenterWirelessAvailable = false;
    int m_controlCenterBluetoothConnectedDeviceCount = 0;
    bool m_controlCenterBatteryAvailable = false;
    bool m_controlCenterBatteryOnAcPower = false;
    bool m_debugSimulatedBatteryAvailable = false;
    int m_debugSimulatedBatteryPercentage = 72;
    bool m_debugSimulatedBatteryCharging = false;
    bool m_debugSimulatedBatteryOnAcPower = false;
    bool m_controlCenterNightLightEnabled = false;
    bool m_controlCenterNightLightAvailable = false;
    QString m_controlCenterPowerCommand = QStringLiteral("wlogout");
    QString m_controlCenterSettingsCommand = QStringLiteral("systemsettings");
    QString m_controlCenterDiskUsagePath = QStringLiteral("/");
    int m_barHeight = 56;
    int m_barLayerSpacing = 0;
    int m_barLayerSpacingTop = 0;
    int m_barLayerSpacingBottom = 0;
    int m_barLayerSpacingLeft = 0;
    int m_barLayerSpacingRight = 0;
    bool m_agendaInstalled = false;
    QString m_weatherIconName = QStringLiteral("weather-severe-alert");
    QString m_weatherTemperature = QStringLiteral("--°C");
    QString m_weatherConditionLabel;
    QString m_weatherLocationName;
    double m_weatherLatitude = 40.7128;
    double m_weatherLongitude = -74.0060;
    QString m_weatherTemperatureUnit = QStringLiteral("celsius");
    int m_weatherRefreshMinutes = 20;
    QString m_userConfigPath;
    QLocalSocket *m_hyprlandEventSocket = nullptr;
    QByteArray m_hyprlandEventBuffer;
    QTimer *m_workspaceRefreshTimer = nullptr;
    QTimer *m_focusedWindowRefreshTimer = nullptr;
    QTimer *m_mprisRefreshTimer = nullptr;
    QTimer *m_mprisPlaybackTimer = nullptr;
    QNetworkAccessManager *m_weatherNetwork = nullptr;
    QTimer *m_weatherRefreshTimer = nullptr;
    QTimer *m_controlCenterStatusTimer = nullptr;
    bool m_controlCenterRuntimeActive = false;
    QString m_mprisServiceName;
    QString m_mprisPropertiesServiceName;
    qint64 m_mprisTrackLengthUs = 0;
    qint64 m_mprisPositionUs = 0;
    qint64 m_mprisLastPositionUs = 0;
    qint64 m_mprisLastPositionEpochMs = 0;
};
