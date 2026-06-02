// SPDX-License-Identifier: BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

Maui.ApplicationWindow
{
    id: root
    title: "Valenz Prototype"
    color: "transparent"
    background: null

    Maui.WindowBlur
    {
        view: root
        geometry: Qt.rect(0, 0, root.width, root.height)
        windowRadius: Maui.Style.radiusV
        enabled: true
    }

    Rectangle
    {
        anchors.fill: parent
        color: Maui.Theme.backgroundColor
        opacity: 0.76
        radius: Maui.Style.radiusV
    }

    function traceMenu(action, detail)
    {
        if (detail === undefined)
            detail = ""

        if (valenzBridge)
            valenzBridge.trace("overflow_menu", action, detail)
    }

    function cleanText(value)
    {
        return String(value || "").trim()
    }

    function controlCenterIconMode()
    {
        const mode = String(valenzBridge ? valenzBridge.controlCenterIconMode : "auto").trim().toLowerCase()
        return mode.length > 0 ? mode : "auto"
    }

    function controlCenterScreenDpi()
    {
        if (root.screen && root.screen.pixelDensity > 0)
            return root.screen.pixelDensity * 25.4
        return Screen.pixelDensity * 25.4
    }

    function controlCenterPrototypeNetworkState()
    {
        const state = String(valenzBridge ? valenzBridge.prototypeNetworkState : "auto").trim().toLowerCase()
        switch (state)
        {
            case "wired":
            case "wireless":
            case "hotspot":
            case "vpn":
            case "cellular":
            case "offline":
                return state
            default:
                return "auto"
        }
    }

    function controlCenterPrototypeBluetoothState()
    {
        const state = String(valenzBridge ? valenzBridge.prototypeBluetoothState : "auto").trim().toLowerCase()
        return (state === "on" || state === "off") ? state : "auto"
    }

    function controlCenterPrototypeVolumeState()
    {
        const state = String(valenzBridge ? valenzBridge.prototypeVolumeState : "auto").trim().toLowerCase()
        switch (state)
        {
            case "muted":
            case "low":
            case "medium":
            case "high":
                return state
            default:
                return "auto"
        }
    }

    function controlCenterButtonGlyph(iconName)
    {
        switch (iconName)
        {
            // Cask-style FA glyphs (4-hex escapes) render consistently in QML.
            case "network-wired": return "\uF0E8"
            case "network-wireless": return "\uF1EB"
            case "network-wireless-hotspot": return "\uF1EB"
            case "network-vpn": return "\uF023"
            case "network-cellular-3g": return "\uF10B"
            case "network-disconnect": return "\uF127"
            case "bluetooth-active": return "\uF294"
            case "bluetooth-disabled": return "\uF293"
            case "audio-volume-muted": return "\uF026"
            case "audio-volume-high": return "\uF028"
            case "audio-volume-medium": return "\uF027"
            case "audio-volume-low": return "\uF026"
            case "battery": return "\uF240"
            case "battery-full": return "\uF240"
            case "battery-100": return "\uF240"
            case "battery-good": return "\uF241"
            case "battery-080": return "\uF241"
            case "battery-medium": return "\uF242"
            case "battery-060": return "\uF242"
            case "battery-040": return "\uF242"
            case "battery-low": return "\uF243"
            case "battery-caution": return "\uF243"
            case "battery-020": return "\uF243"
            case "battery-empty": return "\uF244"
            case "battery-missing": return "\uF244"
            case "battery-000": return "\uF244"
            case "battery-charging": return "\uEEA1"
            case "battery-full-charging": return "\uEEA1"
            case "battery-good-charging": return "\uEEA1"
            case "battery-medium-charging": return "\uEEA1"
            case "battery-low-charging": return "\uEEA1"
            case "battery-caution-charging": return "\uEEA1"
            case "battery-charging-080": return "\uEEA1"
            case "battery-charging-060": return "\uEEA1"
            case "battery-charging-040": return "\uEEA1"
            case "battery-charging-020": return "\uEEA1"
            case "power-profile-performance": return "\uF0E7"
            case "power-profile-balanced": return "\uEEB2"
            case "power-profile-power-saver": return "\uF06C"
            case "notifications": return "\uF0F3"
            case "notifications-disabled": return "\uF0A2"
            default: return "\uF128"
        }
    }

    function controlCenterButtonGlyphColor(kind)
    {
        switch (kind)
        {
            case "network": return Maui.Theme.textColor
            case "bluetooth": return Maui.Theme.textColor
            case "volume": return Maui.Theme.textColor
            default: return Maui.Theme.textColor
        }
    }

    property date nowDateTime: new Date()
    readonly property string weatherIconName: root.cleanText(valenzBridge ? valenzBridge.weatherIconName : "weather-severe-alert")
    readonly property string weatherTemperature: root.cleanText(valenzBridge ? valenzBridge.weatherTemperature : "--°C")
    readonly property string weatherLocationName: root.cleanText(valenzBridge ? valenzBridge.weatherLocationName : "")
    readonly property string clockText: Qt.formatTime(nowDateTime, "hh:mm")
    readonly property string dateText: Qt.formatDate(nowDateTime, "ddd, MMM d")
    readonly property bool mprisModuleVisible: valenzBridge ? (valenzBridge.mprisAlwaysVisible || valenzBridge.mprisVisible) : true
    readonly property string controlCenterVolumePercentageText: root.cleanText(valenzBridge ? valenzBridge.controlCenterVolumePercentage : "")
    readonly property bool controlCenterBatteryCharging: valenzBridge ? valenzBridge.controlCenterBatteryCharging : false
    readonly property string controlCenterBatteryPercentageText: root.cleanText(valenzBridge ? valenzBridge.controlCenterBatteryPercentage : "")
    readonly property string controlCenterBrightnessPercentageText: root.cleanText(valenzBridge ? valenzBridge.controlCenterBrightnessPercentage : "")
    readonly property bool controlCenterBrightnessAvailable: valenzBridge ? valenzBridge.controlCenterBrightnessAvailable : false
    readonly property string controlCenterNetworkState:
    {
        const state = root.cleanText(valenzBridge ? valenzBridge.controlCenterNetworkState : "offline").toLowerCase()
        return (state === "wired" || state === "wireless" || state === "offline") ? state : "offline"
    }
    readonly property bool controlCenterBluetoothEnabled: valenzBridge ? valenzBridge.controlCenterBluetoothEnabled : false
    readonly property bool controlCenterBluetoothAvailable: valenzBridge ? valenzBridge.controlCenterBluetoothAvailable : false
    readonly property bool controlCenterVolumeMuted: valenzBridge ? valenzBridge.controlCenterVolumeMuted : false
    readonly property bool controlCenterBatteryAvailable: valenzBridge ? valenzBridge.controlCenterBatteryAvailable : false
    readonly property bool controlCenterBatteryOnAcPower: valenzBridge ? valenzBridge.controlCenterBatteryOnAcPower : false
    readonly property string controlCenterPowerProfileCurrent: root.cleanText(valenzBridge ? valenzBridge.controlCenterPowerProfileCurrent : "balanced").toLowerCase()
    readonly property string controlCenterNetworkIconName:
    {
        if (controlCenterNetworkState === "wired")
            return "network-wired"
        if (controlCenterNetworkState === "wireless")
            return "network-wireless"
        return "network-disconnect"
    }
    readonly property string controlCenterBluetoothIconName: controlCenterBluetoothEnabled ? "bluetooth-active" : "bluetooth-disabled"
    readonly property string controlCenterVolumeIconName:
    {
        let percentage = parseInt(root.controlCenterVolumePercentageText, 10)
        if (isNaN(percentage))
            percentage = 0
        percentage = Math.max(0, Math.min(100, percentage))

        if (root.controlCenterVolumeMuted || percentage <= 0)
            return "audio-volume-muted"
        if (percentage < 33)
            return "audio-volume-low"
        if (percentage < 66)
            return "audio-volume-medium"
        return "audio-volume-high"
    }
    readonly property string controlCenterPowerProfileIconName:
    {
        switch (controlCenterPowerProfileCurrent)
        {
            case "performance": return "power-profile-performance"
            case "power-saver": return "power-profile-power-saver"
            case "balanced":
            default: return "power-profile-balanced"
        }
    }
    readonly property string controlCenterBatteryIconName:
    {
        let percentage = parseInt(root.controlCenterBatteryPercentageText, 10)
        if (isNaN(percentage))
            percentage = 0
        percentage = Math.max(0, Math.min(100, percentage))

        if (root.controlCenterBatteryOnAcPower)
        {
            if (percentage >= 95) return "battery-full-charging"
            if (percentage >= 75) return "battery-good-charging"
            if (percentage >= 45) return "battery-medium-charging"
            if (percentage >= 20) return "battery-low-charging"
            return "battery-caution-charging"
        }

        if (percentage >= 95) return "battery-full"
        if (percentage >= 75) return "battery-good"
        if (percentage >= 45) return "battery-medium"
        if (percentage >= 20) return "battery-low"
        if (percentage > 0) return "battery-caution"
        return "battery-empty"
    }
    readonly property bool controlCenterUseSystemThemeIcons:
        {
            const mode = root.controlCenterIconMode()
            if (mode === "system16")
                return true
            if (mode === "nerd")
                return false
            return root.controlCenterScreenDpi() <= 120
        }


    Timer
    {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.nowDateTime = new Date()
    }


    ControlCenter
    {
        id: _controlCenterPopup
        parent: Overlay.overlay
        anchorButton: _controlCenterButton
        rootWindow: root
        bridge: valenzBridge
        notificationsControllerRef: notificationsController
    }

    NotificationsCenter
    {
        id: _notificationsCenterPopup
        controller: notificationsController
        parent: Overlay.overlay
        anchorButton: _notificationsCenterButton
        rootWindow: root
        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
    }

    NotificationsBubble
    {
        id: _notificationsBubble
        parent: Overlay.overlay
        anchorButton: _notificationsCenterButton
        rootWindow: root
        controller: notificationsController
        notificationsPopup: _notificationsCenterPopup
        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
    }

    CalendarPopup
    {
        id: _calendarPopup
        parent: Overlay.overlay
        anchorItem: _weatherClock
        rootWindow: root
        bridge: valenzBridge
    }

    SettingsDialog
    {
        id: _settingsDialog
        parent: Overlay.overlay
        bridge: valenzBridge
    }

    Connections
    {
        target: notificationsController

        function onTransientNotification(id, sourceName, messageText, timestampText, iconName, urgencyLevel, actionText, actionKey)
        {
            console.log("main.onTransientNotification", id, sourceName, "dnd=", notificationsController ? notificationsController.dndEnabled : false, "centerVisible=", _notificationsCenterPopup.visible, "bubbleVisible=", _notificationsBubble.visible)
            if (_notificationsCenterPopup.visible)
                return

            _notificationsBubble.showNotification(id, sourceName, messageText, timestampText, iconName, urgencyLevel, actionText, actionKey)
        }
    }

    Maui.PageLayout
    {
        id: _pageLayout
        anchors.fill: parent
        clip: true

        split: false

        altHeader: Maui.Handy.isMobile
        Maui.Controls.showCSD: true

        headBar.visible: true
        headBar.forceCenterMiddleContent: false
        headerMargins: Maui.Handy.isMobile ? 0 : Maui.Style.contentMargins
        footerMargins: headerMargins

        Maui.Theme.colorSet: Maui.Theme.View
        background: null

        leftContent:  [
            WorkspaceBadge
            {
                bridge: valenzBridge
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            },

            WorkspaceNavigation
            {
                bridge: valenzBridge
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
                visible: root.mprisModuleVisible
            },

            MprisControl
            {
                id: _mprisControl
                bridge: valenzBridge
                visible: root.mprisModuleVisible
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            }
        ]

        rightContent: [
            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            },

            SystemTray
            {
                controller: systemTrayController
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            },

            NotificationsCenterButton
            {
                id: _notificationsCenterButton
                popup: _notificationsCenterPopup
                useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
                iconName: notificationsController && notificationsController.dndEnabled ? "notifications-disabled" : "notifications"
                glyphForIcon: root.controlCenterButtonGlyph
                countText: String(Math.max(0, notificationsController ? notificationsController.count : 0))
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            },

            ControlCenterButton
            {
                id: _controlCenterButton
                popup: _controlCenterPopup
                useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
                networkIconName: root.controlCenterNetworkIconName
                bluetoothIconName: root.controlCenterBluetoothIconName
                bluetoothAvailable: root.controlCenterBluetoothAvailable
                volumeIconName: root.controlCenterVolumeIconName
                volumePercentageText: root.controlCenterVolumePercentageText
                batteryIconName: root.controlCenterBatteryIconName
                batteryPercentageText: root.controlCenterBatteryPercentageText
                batteryAvailable: root.controlCenterBatteryAvailable
                powerProfileIconName: root.controlCenterPowerProfileIconName
                glyphForIcon: root.controlCenterButtonGlyph
                glyphColorForKind: root.controlCenterButtonGlyphColor
            },

            ToolSeparator
            {
                topPadding: 10
                bottomPadding: 10
            },

            Loader
            {
                id: _mainMenuLoader
                asynchronous: true
                active: true
                visible: true
                sourceComponent: Maui.ToolButtonMenu
                {
                    id: _mainMenu
                    icon.name: "overflow-menu"

                    MenuItem
                    {
                        text: "Shortcuts"
                        onTriggered: root.traceMenu("dialog/shortcuts")
                    }

                    MenuItem
                    {
                        text: "Settings"
                        onTriggered:
                        {
                            root.traceMenu("dialog/settings")
                            _settingsDialog.open()
                        }
                    }

                    MenuItem
                    {
                        text: "About"
                        icon.name: "documentinfo"
                        onTriggered:
                        {
                            root.traceMenu("dialog/about")
                            Maui.App.aboutDialog()
                        }
                    }
                }
            }
        ]

        headBar.middleContent: RowLayout
        {
            Layout.fillWidth: true
            spacing: 0

            Item
            {
                id: _middleContentArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Maui.Style.units.gridUnit * 12

                readonly property real leftSectionWidth:
                {
                    const bar = _pageLayout.headBar
                    if (!bar || !bar.leftLayout || !bar.farLeftLayout)
                        return 0
                    return bar.leftLayout.implicitWidth + bar.farLeftLayout.implicitWidth
                }

                readonly property real rightSectionWidth:
                {
                    const bar = _pageLayout.headBar
                    if (!bar || !bar.rightLayout || !bar.farRightLayout)
                        return 0
                    return bar.rightLayout.implicitWidth + bar.farRightLayout.implicitWidth
                }

                readonly property real globalCenterOffset: (rightSectionWidth - leftSectionWidth) / 2

                WindowTitle
                {
                    id: _windowTitleMiddle
                    bridge: valenzBridge
                    referenceHeight: _mprisControl.actionsButtonHeight
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width:
                    {
                        const available = _weatherBlock.x - Maui.Style.space.medium - x
                        return Math.max(0, Math.min(implicitWidth, available))
                    }
                }

                RowLayout
                {
                    id: _weatherBlock
                    spacing: Maui.Style.space.small
                    anchors.verticalCenter: parent.verticalCenter
                    x:
                    {
                        const centeredX = (parent.width - width) / 2
                        const shiftedX = centeredX + _middleContentArea.globalCenterOffset
                        const minX = 0
                        const maxX = Math.max(0, parent.width - width)
                        return Math.min(maxX, Math.max(minX, shiftedX))
                    }

                    ToolSeparator
                    {
                        topPadding: 10
                        bottomPadding: 10
                    }

                    WeatherClock
                    {
                        id: _weatherClock
                        clockText: root.clockText
                        dateText: root.dateText
                        weatherIconName: root.weatherIconName
                        weatherTemperature: root.weatherTemperature
                        weatherLocationName: root.weatherLocationName

                        TapHandler
                        {
                            onTapped:
                            {
                                _calendarPopup.toggleFromAnchor()
                            }
                        }
                    }

                    ToolSeparator
                    {
                        topPadding: 10
                        bottomPadding: 10
                    }
                }
            }
        }
    }
}
