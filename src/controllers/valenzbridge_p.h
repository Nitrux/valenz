#pragma once

constexpr auto kControlCenterPowerCommandKey = "ControlCenter/powerCommand";
constexpr auto kControlCenterSettingsCommandKey = "ControlCenter/settingsCommand";
constexpr auto kControlCenterDiskUsagePathKey = "ControlCenter/diskUsagePath";
constexpr auto kWindowBarHeightKey = "Window/barHeight";
constexpr auto kWindowBarWidthKey = "Window/barWidth";
constexpr auto kWindowBarLayerSpacingKey = "Window/barLayerSpacing";
constexpr auto kSystemTrayDebugDetailsKey = "SystemTray/debugDetails";
constexpr auto kLegacyWindowPopupMaxWidthKey = "Window/popupMaxWidth";
constexpr auto kWindowBarHeightMax = 100;
constexpr auto kFullscreenModeNone = 0;
constexpr auto kFullscreenModeMaximized = 1;
constexpr auto kFullscreenModeFullscreen = 2;
constexpr auto kFullscreenModeMax = kFullscreenModeMaximized | kFullscreenModeFullscreen;
constexpr auto kDistroConfigPath = "/etc/valenz/valenz.conf";
constexpr auto kFocusedWindowIconNameKey = "Window/focusedWindowIconName";
constexpr auto kControlCenterIconModeKey = "ControlCenter/iconMode";
constexpr auto kWeatherLatitudeKey = "Weather/latitude";
constexpr auto kWeatherLongitudeKey = "Weather/longitude";
constexpr auto kWeatherTemperatureUnitKey = "Weather/temperatureUnit";
constexpr auto kWeatherRefreshMinutesKey = "Weather/refreshMinutes";
constexpr auto kMprisAlwaysVisibleKey = "Mpris/alwaysVisible";
constexpr auto kOpenMeteoUrl = "https://api.open-meteo.com/v1/forecast";

constexpr auto kMprisServicePrefix = "org.mpris.MediaPlayer2.";
constexpr auto kMprisObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto kMprisPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto kDbusPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto kDbusService = "org.freedesktop.DBus";
constexpr auto kDbusPath = "/org/freedesktop/DBus";
constexpr auto kDbusInterface = "org.freedesktop.DBus";
constexpr auto kMprisRefreshIntervalMs = 4000;
constexpr auto kMprisPlaybackTickMs = 250;

constexpr auto kLegacyFocusedWindowIconNameKey = "window/iconName";
constexpr auto kLegacyControlCenterIconModeKey = "controlCenter/iconMode";
constexpr auto kLegacyControlCenterPowerCommandKey = "controlCenter/powerCommand";
constexpr auto kLegacyControlCenterDiskUsagePathKey = "controlCenter/diskUsagePath";
constexpr auto kWeatherRefreshMinMinutes = 5;
constexpr auto kWeatherRefreshMaxMinutes = 180;

#include "valenzbridge_helpers_inline.h"
