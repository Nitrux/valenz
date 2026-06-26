#pragma once

constexpr auto kControlCenterPowerCommandKey = "ControlCenter/powerCommand";
constexpr auto kControlCenterSettingsCommandKey = "ControlCenter/settingsCommand";
constexpr auto kControlCenterDiskUsagePathKey = "ControlCenter/diskUsagePath";
constexpr auto kWindowBarHeightKey = "Window/barHeight";
constexpr auto kWindowBarLayerSpacingKey = "Window/barLayerSpacing";
constexpr auto kWindowBarLayerSpacingTopKey = "Window/barLayerSpacingTop";
constexpr auto kWindowBarLayerSpacingBottomKey = "Window/barLayerSpacingBottom";
constexpr auto kWindowBarLayerSpacingLeftKey = "Window/barLayerSpacingLeft";
constexpr auto kWindowBarLayerSpacingRightKey = "Window/barLayerSpacingRight";
constexpr auto kSystemTrayDebugDetailsKey = "Debug/debugDetails";
constexpr auto kDebugSimulatedBrightnessAvailableKey = "Debug/simulateBrightnessAvailable";
constexpr auto kDebugSimulatedBrightnessPercentageKey = "Debug/simulateBrightnessPercentage";
constexpr auto kDebugSimulatedBatteryAvailableKey = "Debug/simulateBatteryAvailable";
constexpr auto kDebugSimulatedBatteryPercentageKey = "Debug/simulateBatteryPercentage";
constexpr auto kDebugSimulatedBatteryChargingKey = "Debug/simulateBatteryCharging";
constexpr auto kDebugSimulatedBatteryOnAcPowerKey = "Debug/simulateBatteryOnAcPower";
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
constexpr auto kBluezService = "org.bluez";
constexpr auto kBluezObjectManagerInterface = "org.freedesktop.DBus.ObjectManager";
constexpr auto kBluezDeviceInterface = "org.bluez.Device1";
constexpr auto kBluezMediaPlayerInterface = "org.bluez.MediaPlayer1";
constexpr auto kMprisRefreshIntervalMs = 15000;
constexpr auto kMprisPlaybackTickMs = 250;

constexpr auto kLegacyFocusedWindowIconNameKey = "window/iconName";
constexpr auto kLegacyControlCenterIconModeKey = "controlCenter/iconMode";
constexpr auto kLegacyControlCenterPowerCommandKey = "controlCenter/powerCommand";
constexpr auto kLegacyControlCenterDiskUsagePathKey = "controlCenter/diskUsagePath";
constexpr auto kLegacySystemTrayDebugDetailsKey = "SystemTray/debugDetails";
constexpr auto kWeatherRefreshMinMinutes = 5;
constexpr auto kWeatherRefreshMaxMinutes = 180;

#include "valenzbridge_helpers_inline.h"
