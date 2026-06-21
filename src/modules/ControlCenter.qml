import QtQuick
import QtQuick.Controls
import QtQuick.Controls 2.15 as QQC
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{

    id: controlCenter

    property Item anchorButton
    property var rootWindow
    property QtObject bridge
    property QtObject notificationsControllerRef
    property Item overlayItem

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted: { }
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Keys.onEscapePressed:
    {
        close()
    }

    onClosing: function(closeEvent)
    {
        if (_fadeOutPending)
            return

        closeEvent.accepted = false
        close()
    }



    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInsetX: 8
    readonly property int _panelInsetY: 8
    // readonly property color _panelColor: Qt.alpha(Maui.Theme.backgroundColor, 0.76)
    readonly property color _panelColor: Maui.Theme.backgroundColor
    readonly property string _nerdFontFamily: "Symbols Nerd Font Mono"
    readonly property int _glyphSize: Math.max(15, Math.round(Maui.Style.iconSizes.medium * 0.9))
    readonly property int _cardPadding: Maui.Style.space.small
    readonly property int _toolActionMinSize: Math.max(28, Maui.Style.toolBarHeightAlt)
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    property bool _cellularEnabled: false
    readonly property bool _dndEnabled: controlCenter.notificationsControllerRef ? controlCenter.notificationsControllerRef.dndEnabled : false
    readonly property bool _nightLightEnabled: controlCenter.bridge ? controlCenter.bridge.controlCenterNightLightEnabled : false
    readonly property bool _nightLightAvailable: controlCenter.bridge ? controlCenter.bridge.controlCenterNightLightAvailable : false
    readonly property int _volumePercentage:
    {
        const text = controlCenter.bridge ? controlCenter.bridge.controlCenterVolumePercentage : "0%"
        const parsed = parseInt(String(text || "").trim(), 10)
        return isNaN(parsed) ? 0 : Math.max(0, Math.min(100, parsed))
    }
    readonly property int _brightnessPercentage:
    {
        const text = controlCenter.bridge ? controlCenter.bridge.controlCenterBrightnessPercentage : "0%"
        const parsed = parseInt(String(text || "").trim(), 10)
        return isNaN(parsed) ? 0 : Math.max(0, Math.min(100, parsed))
    }
    readonly property bool _brightnessAvailable: controlCenter.bridge ? controlCenter.bridge.controlCenterBrightnessAvailable : false
    readonly property string _powerProfileIconName:
    {
        const profile = controlCenter.bridge ? String(controlCenter.bridge.controlCenterPowerProfileCurrent).toLowerCase() : "balanced"
        switch (profile)
        {
            case "performance": return "power-profile-performance"
            case "power-saver": return "power-profile-power-saver"
            case "balanced":
            default: return "power-profile-balanced"
        }
    }
    property int _geometryRevision: 0
    property bool _systemResourcesRefreshActive: false
    property bool _fadeOutPending: false
    property bool _panelOpen: false

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100
    readonly property real _targetY:
    {
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorButton)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return targetY
    }
    readonly property real _availableHeightFromAnchor:
    {
        const screenGeometry = controlCenter._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _panel.implicitHeight

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorButton)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        const startY = Math.max(minY, targetY)
        return Math.max(0, screenGeometry.height - startY - _margin)
    }

    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _anchorPointInScreen(offsetX, offsetY)
    {
        if (!anchorButton || !anchorButton.mapToGlobal)
        {
            return null
        }

        const globalPoint = anchorButton.mapToGlobal(offsetX, offsetY)
        if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
        {
            return globalPoint
        }
        return null
    }

    function _screenGeometry()
    {
        const screen = controlCenter.rootWindow ? controlCenter.rootWindow.screen : null
        if (screen && screen.availableGeometry && screen.availableGeometry.width > 0 && screen.availableGeometry.height > 0)
            return screen.availableGeometry
        if (screen && screen.geometry && screen.geometry.width > 0 && screen.geometry.height > 0)
            return screen.geometry
        if (screen && screen.width > 0 && screen.height > 0)
            return Qt.rect(0, 0, screen.width, screen.height)
        if (screen && screen.virtualGeometry && screen.virtualGeometry.width > 0 && screen.virtualGeometry.height > 0)
            return screen.virtualGeometry

        return Qt.rect(0, 0, 0, 0)
    }

    function _powerProfileLabel(profileName)
    {
        switch (profileName)
        {
            case "power-saver": return "Power Saver"
            case "balanced": return "Balanced"
            case "performance": return "Performance"
            default: return profileName
        }
    }

    function _networkModeTitle()
    {
        const state = controlCenter.bridge ? String(controlCenter.bridge.controlCenterNetworkMode).toLowerCase() : "wireless"
        return state === "wired" ? "Wired" : "WiFi"
    }

    function _networkModeSubtitle()
    {
        const state = controlCenter.bridge ? String(controlCenter.bridge.controlCenterNetworkMode).toLowerCase() : "wireless"
        if (state === "offline")
            return "Off"
        if (state === "wired")
            return "Connected"
        if (state === "hotspot")
            return "Hotspot"
        if (state === "vpn")
            return "VPN"
        if (state === "cellular")
            return "Cellular"
        return "Connected"
    }

    function _commitVolumeFromSlider()
    {
        if (controlCenter.bridge)
            controlCenter.bridge.setControlCenterVolumePercentageFromSlider(Math.round(_volumeSlider.value))
    }

    function _commitBrightnessFromSlider()
    {
        if (controlCenter.bridge && controlCenter._brightnessAvailable)
            controlCenter.bridge.setControlCenterBrightnessPercentageFromSlider(Math.round(_brightnessSlider.value))
    }

    function _controlCenterCpuPercentage()
    {
        const bridge = controlCenter.bridge
        if (!bridge)
            return 0
        return Math.max(0, Math.min(100, Number(bridge.controlCenterCpuPercentage) || 0))
    }

    function _controlCenterRamPercentage()
    {
        const bridge = controlCenter.bridge
        if (!bridge)
            return 0
        return Math.max(0, Math.min(100, Number(bridge.controlCenterRamPercentage) || 0))
    }

    function _controlCenterDiskUsageText()
    {
        const bridge = controlCenter.bridge
        if (!bridge)
            return "-- / --"
        const text = String(bridge.controlCenterDiskUsageText || "").trim()
        return text.length > 0 ? text : "-- / --"
    }

    function _controlCenterDiskUsagePercentage()
    {
        const bridge = controlCenter.bridge
        if (!bridge)
            return 0
        return Math.max(0, Math.min(100, Number(bridge.controlCenterDiskUsagePercentage) || 0))
    }

    Timer
    {
        id: _volumeApplyTimer
        interval: 150
        repeat: false
        onTriggered: controlCenter._commitVolumeFromSlider()
    }

    Timer
    {
        id: _brightnessApplyTimer
        interval: 150
        repeat: false
        onTriggered: controlCenter._commitBrightnessFromSlider()
    }

    Timer
    {
        id: _systemResourcesRefreshTimer
        interval: 1000
        repeat: true
        running: controlCenter._systemResourcesRefreshActive
        onTriggered:
        {
            if (controlCenter.bridge)
                controlCenter.bridge.refreshControlCenterSystemResources()
        }
    }

    signal aboutToShow()
    signal opened()
    signal closed()

    function open()
    {
        if (visible)
            return
        aboutToShow()
        _fadeOutPending = false
        _panelOpen = false
        visible = true
        requestActivate()
        Qt.callLater(function()
        {
            if (visible)
                _panelOpen = true
        })
        _logPopupGeometry("opened")
    }

    function close()
    {
        if (!visible)
            return

        _fadeOutPending = true
        _panelOpen = false
        _fadeOutTimer.restart()
    }

    function _logPopupGeometry(reason)
    {
    }

    Timer
    {
        id: _deferredGeometryRefreshTimer
        interval: 32
        repeat: false
        onTriggered:
        {
            if (visible)
            {
                _touchGeometryRevision()
                _logPopupGeometry("deferred")
            }
        }
    }

    Timer
    {
        id: _fadeOutTimer
        interval: controlCenter._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (controlCenter._fadeOutPending)
            {
                controlCenter._fadeOutPending = false
                visible = false
            }
        }
    }

    width: Math.max(_panel.implicitWidth, _minPanelWidth)
    height: Math.min(_panel.implicitHeight, _availableHeightFromAnchor)

    onVisibleChanged:
    {
        if (visible)
        {
            opened()
            Qt.callLater(_touchGeometryRevision)
            _deferredGeometryRefreshTimer.restart()
            _logPopupGeometry("visible")
            _systemResourcesRefreshActive = true
            if (bridge)
                bridge.refreshControlCenterSystemResources()
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            closed()
            _systemResourcesRefreshActive = false
        }
    }
    x:
    {
        const dep = _geometryRevision
        const screenGeometry = controlCenter._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        const minX = _margin
        const maxX = Math.max(minX, screenGeometry.width - width - _margin)
        let targetX = maxX

        if (anchorButton)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetX = p.x - width
        }

        const finalX = Math.max(minX, Math.min(maxX, targetX))
        return finalX
    }
    y:
    {
        const dep = _geometryRevision
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorButton)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        const finalY = Math.max(minY, targetY)
        return finalY
    }

    Connections
    {
        target: controlCenter.anchorButton

        function onXChanged() { controlCenter._touchGeometryRevision() }
        function onYChanged() { controlCenter._touchGeometryRevision() }
        function onWidthChanged() { controlCenter._touchGeometryRevision() }
        function onHeightChanged() { controlCenter._touchGeometryRevision() }
        function onVisibleChanged() { controlCenter._touchGeometryRevision() }
    }

    Connections
    {
        target: controlCenter.overlayItem

        function onWidthChanged() { controlCenter._touchGeometryRevision() }
        function onHeightChanged() { controlCenter._touchGeometryRevision() }
        function onXChanged() { controlCenter._touchGeometryRevision() }
        function onYChanged() { controlCenter._touchGeometryRevision() }
    }

    Connections
    {
        target: controlCenter.rootWindow

        function onWidthChanged() { controlCenter._touchGeometryRevision() }
        function onHeightChanged() { controlCenter._touchGeometryRevision() }
        function onVisibilityChanged() { controlCenter._touchGeometryRevision() }
        function onWindowStateChanged() { controlCenter._touchGeometryRevision() }
    }

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        opacity: 0.0
        scale: 0.97
        transformOrigin: Item.Center
        implicitWidth: Math.max(controlCenter._minPanelWidth, _panelContent.implicitWidth + (controlCenter._panelInsetX * 2))
        implicitHeight: _panelContent.implicitHeight + (controlCenter._panelInsetY * 2)
        radius: Maui.Style.radiusV
        color: controlCenter._panelColor
        states: [
            State
            {
                name: "open"
                when: controlCenter._panelOpen
                PropertyChanges
                {
                    target: _panel
                    opacity: 1.0
                    scale: 1.0
                }
            },
            State
            {
                name: "closed"
                when: !controlCenter._panelOpen
                PropertyChanges
                {
                    target: _panel
                    opacity: 0.0
                    scale: 0.97
                }
            }
        ]
        transitions: Transition
        {
            reversible: true
            NumberAnimation
            {
                properties: "opacity,scale"
                duration: controlCenter._panelOpen ? controlCenter._fadeInDurationMs : controlCenter._fadeOutDurationMs
                easing.type: Easing.InOutCubic
            }
        }
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: "#80000000"
        }

        Flickable
        {
            id: _panelFlick
            anchors.fill: parent
            anchors.leftMargin: controlCenter._panelInsetX
            anchors.rightMargin: controlCenter._panelInsetX
            anchors.topMargin: controlCenter._panelInsetY
            anchors.bottomMargin: controlCenter._panelInsetY
            contentWidth: width
            contentHeight: _panelContent.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ColumnLayout
        {
            id: _panelContent
            width: _panelFlick.width
            spacing: Maui.Style.space.small

            GridLayout
            {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Maui.Style.space.small
                columnSpacing: Maui.Style.space.small

                Maui.SectionItem
                {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Rectangle
                            {
                                implicitWidth: Maui.Style.iconSizes.big + Maui.Style.space.small
                                implicitHeight: implicitWidth
                                radius: implicitWidth / 2
                                color: Maui.Theme.alternateBackgroundColor

                                Label
                                {
                                    anchors.centerIn: parent
                                    text: "\uf007"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pixelSize: controlCenter._glyphSize
                                    color: Maui.Theme.textColor
                                }
                            }

                            Label
                            {
                                text: controlCenter.bridge ? controlCenter.bridge.userRealName : "User"
                                color: Maui.Theme.textColor
                                font.weight: Font.DemiBold
                                verticalAlignment: Text.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            ToolButton
                            {
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                Layout.minimumHeight: 28
                                text: "Power"
                                icon.name: "system-shutdown"
                                display: ToolButton.TextBesideIcon

                                background: Rectangle
                                {
                                    radius: Math.round(Maui.Style.radiusV * 0.6)
                                    color: parent.down ? "#cc3b49" : "#e34b5a"
                                }

                                onClicked:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.executeControlCenterPowerCommand()
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        visible: true
                        enabled: controlCenter.bridge ? controlCenter.bridge.controlCenterNetworkState === "wireless" : false
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Label
                            {
                                text: "\uf1eb"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: controlCenter._networkModeTitle(); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._networkModeSubtitle(); color: Maui.Theme.disabledTextColor }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter.bridge ? controlCenter.bridge.controlCenterNetworkMode !== "offline" : true
                                onToggled:
                                {
                                    if (!controlCenter.bridge)
                                        return

                                    if (checked)
                                    {
                                        const state = String(controlCenter.bridge.controlCenterNetworkMode).toLowerCase()
                                        controlCenter.bridge.controlCenterNetworkMode = state === "wired" ? "wired" : "wireless"
                                    }
                                    else
                                    {
                                        controlCenter.bridge.controlCenterNetworkMode = "offline"
                                    }
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        visible: true
                        enabled: controlCenter.bridge ? controlCenter.bridge.controlCenterBluetoothAvailable : false
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Label
                            {
                                text: "\uf294"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0

                                Label
                                {
                                    text: "Bluetooth"
                                    color: Maui.Theme.textColor
                                    font.weight: Font.DemiBold
                                }
                                Label
                                {
                                    text: controlCenter.bridge && !controlCenter.bridge.controlCenterBluetoothAvailable ? "Unavailable" : (controlCenter.bridge && controlCenter.bridge.controlCenterBluetoothState === "off" ? "Off" : "On")
                                    color: Maui.Theme.disabledTextColor
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter.bridge ? (controlCenter.bridge.controlCenterBluetoothAvailable && controlCenter.bridge.controlCenterBluetoothState !== "off") : false
                                onToggled:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.controlCenterBluetoothState = checked ? "on" : "off"
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        visible: true
                        enabled: controlCenter._nightLightAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Label
                            {
                                text: "\uf186"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: "Night Light"; color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._nightLightAvailable ? (controlCenter._nightLightEnabled ? "On" : "Off") : "Unavailable"; color: Maui.Theme.disabledTextColor }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter._nightLightEnabled
                                enabled: controlCenter._nightLightAvailable
                                onToggled:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.controlCenterNightLightEnabled = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Label
                            {
                                text: "\uf05e"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: "Do Not Disturb"; color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._dndEnabled ? "On" : "Off"; color: Maui.Theme.disabledTextColor }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter._dndEnabled
                                onToggled:
                                {
                                    if (controlCenter.notificationsControllerRef)
                                        controlCenter.notificationsControllerRef.dndEnabled = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Label
                            {
                                text: controlCenter._powerProfileIconName === "power-profile-performance" ? "\uf0e7" : (controlCenter._powerProfileIconName === "power-profile-power-saver" ? "\uf06c" : "\ueeb2")
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                spacing: 0

                                Label
                                {
                                    text: "Power Profile"
                                    color: Maui.Theme.textColor
                                    font.weight: Font.DemiBold
                                }

                                Label
                                {
                                    text: controlCenter._powerProfileLabel(controlCenter.bridge ? controlCenter.bridge.controlCenterPowerProfileCurrent : "balanced")
                                    color: Maui.Theme.disabledTextColor
                                }
                            }

                            Item { Layout.fillWidth: true }

                            QQC.ComboBox
                            {
                                id: _powerProfileSelector
                                readonly property string _fallbackLabel: "Select Profile"
                                readonly property string _longestLabel:
                                {
                                    const entries = controlCenter.bridge ? controlCenter.bridge.controlCenterPowerProfiles : []
                                    let longest = _fallbackLabel

                                    for (let i = 0; i < entries.length; ++i)
                                    {
                                        const candidate = controlCenter._powerProfileLabel(entries[i])
                                        if (candidate.length > longest.length)
                                            longest = candidate
                                    }

                                    return longest
                                }
                                readonly property real _contentDrivenWidth: Math.ceil(_powerProfileLongestLabelMetrics.advanceWidth + (Maui.Style.space.big * 3) + Maui.Style.iconSizes.small)
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                Layout.preferredWidth: _contentDrivenWidth
                                Layout.minimumWidth: _contentDrivenWidth
                                Layout.maximumWidth: _contentDrivenWidth
                                currentIndex: -1
                                enabled: controlCenter.bridge && controlCenter.bridge.controlCenterPowerProfiles.length > 0
                                model: controlCenter.bridge ? controlCenter.bridge.controlCenterPowerProfiles : []
                                displayText: currentIndex === -1 ? _fallbackLabel : controlCenter._powerProfileLabel(currentText)
                                TextMetrics
                                {
                                    id: _powerProfileLongestLabelMetrics
                                    text: _powerProfileSelector._longestLabel
                                    font: _powerProfileSelector.font
                                }
                                delegate: ItemDelegate
                                {
                                    required property string modelData
                                    width: ListView.view ? ListView.view.width : _powerProfileSelector.width
                                    text: controlCenter._powerProfileLabel(modelData)
                                }
                                popup.width:
                                {
                                    const popupContentWidth = popup.contentItem ? popup.contentItem.implicitWidth : 0
                                    const popupFrameWidth = popup.leftPadding + popup.rightPadding
                                    return Math.max(_powerProfileSelector.width, popupContentWidth + popupFrameWidth)
                                }
                                Component.onCompleted:
                                {
                                    if (!controlCenter.bridge)
                                        return

                                    const idx = model.indexOf(controlCenter.bridge.controlCenterPowerProfileCurrent)
                                    currentIndex = idx
                                }
                                Connections
                                {
                                    target: controlCenter.bridge
                                    enabled: !!controlCenter.bridge

                                    function onControlCenterPowerProfilesChanged()
                                    {
                                        if (!controlCenter.bridge)
                                            return

                                        const idx = _powerProfileSelector.model.indexOf(controlCenter.bridge.controlCenterPowerProfileCurrent)
                                        _powerProfileSelector.currentIndex = idx
                                    }

                                    function onControlCenterPowerProfileCurrentChanged()
                                    {
                                        if (!controlCenter.bridge)
                                            return

                                        const idx = _powerProfileSelector.model.indexOf(controlCenter.bridge.controlCenterPowerProfileCurrent)
                                        _powerProfileSelector.currentIndex = idx
                                    }
                                }
                                onActivated:
                                {
                                    if (currentIndex >= 0)
                                    {
                                        if (controlCenter.bridge)
                                            controlCenter.bridge.controlCenterPowerProfileCurrent = currentText
                                    }
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small
                            Label
                            {
                                text: "\uf026"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }
                            Slider
                            {
                                id: _volumeSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._volumePercentage
                                onMoved:
                                {
                                    _volumeApplyTimer.restart()
                                }
                                onPressedChanged:
                                {
                                    if (!pressed)
                                    {
                                        _volumeApplyTimer.stop()
                                        controlCenter._commitVolumeFromSlider()
                                    }
                                }
                                Connections
                                {
                                    target: controlCenter.bridge
                                    enabled: !!controlCenter.bridge

                                    function onControlCenterVolumePercentageChanged()
                                    {
                                        if (!_volumeSlider.pressed)
                                            _volumeSlider.value = controlCenter._volumePercentage
                                    }

                                    function onControlCenterVolumeMutedChanged()
                                    {
                                        if (!_volumeSlider.pressed)
                                            _volumeSlider.value = controlCenter._volumePercentage
                                    }
                                }
                            }
                            Label
                            {
                                text: "\uf028"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        enabled: controlCenter._brightnessAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small
                            Label
                            {
                                text: "\uf042"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }
                            Slider
                            {
                                id: _brightnessSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._brightnessPercentage
                                enabled: controlCenter._brightnessAvailable
                                onMoved:
                                {
                                    _brightnessApplyTimer.restart()
                                }
                                onPressedChanged:
                                {
                                    if (!pressed)
                                    {
                                        _brightnessApplyTimer.stop()
                                        controlCenter._commitBrightnessFromSlider()
                                    }
                                }
                                Connections
                                {
                                    target: controlCenter.bridge
                                    enabled: !!controlCenter.bridge

                                    function onControlCenterBrightnessPercentageChanged()
                                    {
                                        if (!_brightnessSlider.pressed)
                                            _brightnessSlider.value = controlCenter._brightnessPercentage
                                    }

                                    function onControlCenterBrightnessAvailableChanged()
                                    {
                                        _brightnessSlider.enabled = controlCenter._brightnessAvailable
                                        if (!_brightnessSlider.pressed)
                                            _brightnessSlider.value = controlCenter._brightnessPercentage
                                    }
                                }
                            }
                            Label
                            {
                                text: "\uf185"
                                font.family: controlCenter._nerdFontFamily
                                font.pixelSize: controlCenter._glyphSize
                                color: Maui.Theme.textColor
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""

                        ColumnLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Label { text: "\uf2db"; font.family: controlCenter._nerdFontFamily; font.pixelSize: controlCenter._glyphSize; color: Maui.Theme.textColor }
                                Label { text: "CPU"; color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._controlCenterCpuPercentage() + "%"; color: Maui.Theme.disabledTextColor }
                            }
                            ProgressBar
                            {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._controlCenterCpuPercentage()
                            }

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Label { text: "\uefc5"; font.family: controlCenter._nerdFontFamily; font.pixelSize: controlCenter._glyphSize; color: Maui.Theme.textColor }
                                Label { text: "RAM"; color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._controlCenterRamPercentage() + "%"; color: Maui.Theme.disabledTextColor }
                            }
                            ProgressBar
                            {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._controlCenterRamPercentage()
                            }

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Label { text: "\uf0a0"; font.family: controlCenter._nerdFontFamily; font.pixelSize: controlCenter._glyphSize; color: Maui.Theme.textColor }
                                Label { text: "Disk"; color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._controlCenterDiskUsageText(); color: Maui.Theme.disabledTextColor }
                            }
                            ProgressBar
                            {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._controlCenterDiskUsagePercentage()
                            }
                        }
                }
            }
        }
        }
    }
}
