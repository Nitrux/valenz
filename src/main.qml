// SPDX-License-Identifier: BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{
    id: root
    readonly property int barHeight: valenzBridge ? valenzBridge.barHeight : 56
    readonly property int barHeightClamped: Math.max(1, Math.min(barHeight, Screen.height > 0 ? Screen.height : barHeight))
    readonly property int barFrameInset: 6
    readonly property int barContentHeight: Math.max(0, barHeightClamped - (barFrameInset * 2))
    readonly property int barLayerSpacing: valenzBridge ? valenzBridge.barLayerSpacing : 0
    readonly property int barLayerSpacingTop: valenzBridge ? valenzBridge.barLayerSpacingTop : 0
    readonly property int barLayerSpacingBottom: valenzBridge ? valenzBridge.barLayerSpacingBottom : 0
    readonly property int barLayerSpacingLeft: valenzBridge ? valenzBridge.barLayerSpacingLeft : 0
    readonly property int barLayerSpacingRight: valenzBridge ? valenzBridge.barLayerSpacingRight : 0
    readonly property bool systemTrayDebugDetails: valenzBridge ? valenzBridge.systemTrayDebugDetails : false
    readonly property real popupSurfaceOpacity: 0.76
    readonly property color popupSurfaceColor: Qt.lighter(Maui.Theme.backgroundColor, 1.25)
    readonly property int popupSurfaceRadius: Maui.Style.radiusV + 3
    visible: !valenzBridge || !valenzBridge.focusedWindowFullscreen
    readonly property bool useDirectionalSpacing: barLayerSpacingTop > 0 || barLayerSpacingBottom > 0 || barLayerSpacingLeft > 0 || barLayerSpacingRight > 0
    width:
    {
        if (useDirectionalSpacing)
            return Math.max(0, Screen.width - barLayerSpacingLeft - barLayerSpacingRight)

        return Screen.width
    }
    x:
    {
        return 0
    }
    height: barHeightClamped
    minimumHeight: barHeightClamped
    maximumHeight: barHeightClamped
    flags: Qt.FramelessWindowHint | Qt.Tool
    title: i18n("Valenz")
    color: "transparent"

    Maui.WindowBlur
    {
        view: root
        geometry: Qt.rect(0, 0, root.width, barHeightClamped)
        windowRadius: root.popupSurfaceRadius
        enabled: true
    }

    function traceMenu(action, detail)
    {
        if (detail === undefined)
            detail = ""

        if (valenzBridge)
            valenzBridge.trace("overflow_menu", action, detail)
    }

    function openCalendarPopup()
    {
        _calendarPopup.toggleFromAnchor()
    }

    function closeTransientPopups()
    {
        if (_systemTray && _systemTray.closeTrayMenu)
            _systemTray.closeTrayMenu()

        if (_notificationsBubble.visible && _notificationsBubble.forceClose)
            _notificationsBubble.forceClose()
        else if (_notificationsBubble.visible)
            _notificationsBubble.close()

        if (_calendarPopup.visible && _calendarPopup.forceClose)
            _calendarPopup.forceClose()
        else if (_calendarPopup.visible)
            _calendarPopup.close()

        if (_notificationsCenterPopup.visible && _notificationsCenterPopup.forceClose)
            _notificationsCenterPopup.forceClose()
        else if (_notificationsCenterPopup.visible)
            _notificationsCenterPopup.close()

        if (_mprisSourcesPopup.visible && _mprisSourcesPopup.forceClose)
            _mprisSourcesPopup.forceClose()
        else if (_mprisSourcesPopup.visible)
            _mprisSourcesPopup.close()

        if (_controlCenterPopup.visible && _controlCenterPopup.forceClose)
            _controlCenterPopup.forceClose()
        else if (_controlCenterPopup.visible)
            _controlCenterPopup.close()
    }

    function _pointInsideItem(item, x, y)
    {
        if (!item || !item.visible || !item.width || !item.height)
            return false

        const point = item.mapFromItem(_barInner, x, y)
        return point && point.x >= 0 && point.y >= 0 && point.x <= item.width && point.y <= item.height
    }

    function _isPopupToggleArea(x, y)
    {
        return _pointInsideItem(_weatherClock, x, y)
                || _pointInsideItem(_notificationsCenterButton, x, y)
                || _pointInsideItem(_controlCenterButton, x, y)
    }

    function toggleMaximized()
    {
        // Valenz uses a fixed-height bar window, so maximize is a no-op.
    }

    function cleanText(value)
    {
        return String(value || "").trim()
    }

    function controlCenterIconMode()
    {
        const mode = String(valenzBridge ? valenzBridge.controlCenterIconMode : "system16").trim().toLowerCase()
        return (mode === "nerd") ? "nerd" : "system16"
    }

    function controlCenterScreenDpi()
    {
        if (root.screen && root.screen.pixelDensity > 0)
            return root.screen.pixelDensity * 25.4
        return Screen.pixelDensity * 25.4
    }

    function controlCenterButtonGlyphName(iconName)
    {
        switch (iconName)
        {
            case "network-wired": return "\uF0E8"
            case "network-wireless": return "\uF1EB"
            case "network-wireless-hotspot": return "\uF1EB"
            case "network-vpn": return "\uF023"
            case "network-cellular-3g": return "\uF10B"
            case "network-disconnect": return "\uF127"
            case "bluetooth-active": return "\uF293"
            case "bluetooth-disabled": return "\uF294"
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

    function controlCenterButtonGlyph(iconName)
    {
        return root.controlCenterButtonGlyphName(iconName)
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
    readonly property int controlCenterBluetoothConnectedDeviceCount: valenzBridge ? valenzBridge.controlCenterBluetoothConnectedDeviceCount : 0
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
            return mode !== "nerd"
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
        title: i18n("Valenz Control Center")
        anchorButton: _controlCenterButton.popupAnchorMarker
        rootWindow: root
        overlayItem: root.contentItem
        bridge: valenzBridge
        notificationsControllerRef: notificationsController
    }

    NotificationsCenter
    {
        id: _notificationsCenterPopup
        title: i18n("Valenz Notifications Center")
        controller: notificationsController
        anchorButton: _notificationsCenterButton.popupAnchorMarker
        rootWindow: root
        overlayItem: root.contentItem
        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
    }

    NotificationsBubble
    {
        id: _notificationsBubble
        anchorButton: _notificationsCenterButton.popupAnchorMarker
        rootWindow: root
        overlayItem: root.contentItem
        controller: notificationsController
        notificationsPopup: _notificationsCenterPopup
        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
    }

    MprisSourcesPopup
    {
        id: _mprisSourcesPopup
        title: i18n("Valenz MPRIS Controls")
        rootWindow: root
        bridge: valenzBridge
        overlayItem: root.contentItem
        anchorItem: _mprisControl.popupAnchorMarker
    }

    CalendarPopup
    {
        id: _calendarPopup
        title: i18n("Valenz Calendar")
        anchorItem: _weatherClock.popupAnchorMarker
        rootWindow: root
        overlayItem: root.contentItem
        bridge: valenzBridge
    }

    Connections
    {
        target: notificationsController

        function onTransientNotification(id, sourceName, messageText, timestampText, iconName, urgencyLevel, actionText, actionKey)
        {
            if (_notificationsCenterPopup.visible)
                return

            _notificationsBubble.showNotification(id, sourceName, messageText, timestampText, iconName, urgencyLevel, actionText, actionKey)
        }
    }

    Item
    {
        id: _barShell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: barHeightClamped
        Maui.Theme.colorSet: Maui.Theme.View

        Rectangle
        {
            anchors.fill: parent
            color: Maui.Theme.backgroundColor
            opacity: root.popupSurfaceOpacity
            radius: root.popupSurfaceRadius
        }

        Rectangle
        {
            id: _barInner
            anchors.fill: parent
            anchors.margins: barFrameInset
            color: Qt.darker(Maui.Theme.backgroundColor, 1.25)
            radius: Maui.Style.radiusV
            clip: true

            RowLayout
            {
                id: _barLayout
                anchors.fill: parent
                spacing: 0

                Item
                {
                    id: _leftSection
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillHeight: true
                    Layout.leftMargin: Maui.Style.space.small
                    implicitWidth: _leftSectionRow.implicitWidth
                    implicitHeight: _leftSectionRow.implicitHeight

                    RowLayout
                    {
                        id: _leftSectionRow
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0
                        WorkspaceBadge
                        {
                            bridge: valenzBridge
                            padding: Maui.Style.space.small
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        Maui.ToolActions
                        {
                            display: ToolButton.IconOnly
                            checkable: false
                            autoExclusive: false
                            spacing: 2

                            Action
                            {
                                text: i18n("Previous workspace")
                                icon.name: "go-previous"
                                enabled: !!valenzBridge
                                onTriggered: valenzBridge.goToPreviousWorkspace()
                            }

                            Action
                            {
                                text: i18n("Next workspace")
                                icon.name: "go-next"
                                enabled: !!valenzBridge
                                onTriggered: valenzBridge.goToNextWorkspace()
                            }
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        MprisControl
                        {
                            id: _mprisControl
                            bridge: valenzBridge
                            popup: _mprisSourcesPopup
                            visible: root.mprisModuleVisible
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                            visible: root.mprisModuleVisible
                        }
                    }
                }
                
                Item
                {
                    id: _middleContentArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: Maui.Style.units.gridUnit * 12
                    Layout.alignment: Qt.AlignVCenter

                    readonly property real leftSectionWidth: _leftSection.implicitWidth
                    readonly property real rightSectionWidth: _rightSection.implicitWidth
                    readonly property real globalCenterOffset: (rightSectionWidth - leftSectionWidth) / 2

                    WindowTitle
                    {
                        id: _windowTitleMiddle
                        bridge: valenzBridge
                        fallbackTitle: ""
                        referenceHeight: barContentHeight
                        anchors.left: parent.left
                        anchors.leftMargin: Maui.Style.space.small
                        anchors.verticalCenter: parent.verticalCenter
                        width:
                        {
                            const available = _weatherBlock.x - (Maui.Style.space.small * 2) - x
                            return Math.max(0, Math.min(implicitWidth, available))
                        }
                    }

                    RowLayout
                    {
                        id: _weatherBlock
                        spacing: Maui.Style.space.medium
                        anchors.verticalCenter: parent.verticalCenter
                        x:
                        {
                            const centeredX = (parent.width - width) / 2
                            const shiftedX = centeredX + _middleContentArea.globalCenterOffset
                            const minX = 0
                            const maxX = Math.max(0, parent.width - width)
                            return Math.min(maxX, Math.max(minX, shiftedX))
                        }

                        Item
                        {
                            Layout.preferredWidth: Maui.Style.space.small
                            Layout.fillHeight: true
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
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
                                    openCalendarPopup()
                                }
                            }
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                        }
                    }

                    MouseArea
                    {
                        anchors.fill: parent
                        enabled: _controlCenterPopup.visible || _notificationsCenterPopup.visible || _notificationsBubble.visible || _calendarPopup.visible || _mprisSourcesPopup.visible
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        hoverEnabled: false
                        propagateComposedEvents: true

                        onPressed: function(mouse)
                        {
                            if (!_isPopupToggleArea(mouse.x, mouse.y))
                                closeTransientPopups()
                            mouse.accepted = false
                        }
                    }
                }

                Item
                {
                    id: _rightSection
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillHeight: true
                    implicitWidth: _rightSectionRow.implicitWidth
                    implicitHeight: _rightSectionRow.implicitHeight

                    RowLayout
                    {
                        id: _rightSectionRow
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Maui.Style.space.medium
                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                            visible: systemTrayController && systemTrayController.count > 0
                        }

                        SystemTray
                        {
                            id: _systemTray
                            controller: systemTrayController
                            rootWindow: root
                            debugDetails: root.systemTrayDebugDetails
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                            visible: systemTrayController && systemTrayController.count > 0
                        }

                        NotificationsCenterButton
                        {
                            id: _notificationsCenterButton
                            popup: _notificationsCenterPopup
                                                        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
                            iconName: notificationsController && notificationsController.dndEnabled ? "notifications-disabled" : "notifications"
                            glyphForIcon: root.controlCenterButtonGlyph
                            countText: String(Math.max(0, notificationsController ? notificationsController.count : 0))
                        }

                        Maui.Separator
                        {
                            Layout.preferredHeight: 8
                            height: 8
                        }

                        ControlCenterButton
                        {
                            id: _controlCenterButton
                            popup: _controlCenterPopup
                                                        useSystemThemeIcons: root.controlCenterUseSystemThemeIcons
                            networkIconName: root.controlCenterNetworkIconName
                            bluetoothIconName: root.controlCenterBluetoothIconName
                            bluetoothAvailable: root.controlCenterBluetoothAvailable
                            bluetoothConnectedDeviceCount: root.controlCenterBluetoothConnectedDeviceCount
                            volumeIconName: root.controlCenterVolumeIconName
                            volumePercentageText: root.controlCenterVolumePercentageText
                            batteryIconName: root.controlCenterBatteryIconName
                            batteryPercentageText: root.controlCenterBatteryPercentageText
                            batteryAvailable: root.controlCenterBatteryAvailable
                            powerProfileIconName: root.controlCenterPowerProfileIconName
                            glyphForIcon: root.controlCenterButtonGlyph
                            glyphColorForKind: root.controlCenterButtonGlyphColor
                        }
                    }
                }
            }
        }
    }
}
