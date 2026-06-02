import QtQuick
import QtQuick.Controls
import QtQuick.Controls 2.15 as QQC
import QtQuick.Layouts
import QtQuick.Effects

import org.mauikit.controls as Maui

Dialog
{
    id: controlCenter

    property Item anchorButton
    property var rootWindow
    property QtObject bridge
    property QtObject notificationsControllerRef
    property Item overlayItem: controlCenter.parent

    Maui.Theme.colorSet: Maui.Theme.View

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
    readonly property real _targetY:
    {
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorButton)
        {
            const p = _anchorPointInOverlay(0, anchorButton.height)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return targetY
    }
    readonly property real _availableHeightFromAnchor:
    {
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return _panel.implicitHeight

        const minY = _margin
        const startY = Math.max(minY, _targetY)
        return Math.max(0, overlayItem.height - startY - _margin)
    }

    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _anchorPointInOverlay(offsetX, offsetY)
    {
        const overlayItem = controlCenter.parent
        if (!overlayItem || !anchorButton)
            return null

        if (anchorButton.mapToGlobal && overlayItem.mapFromGlobal)
        {
            const globalPoint = anchorButton.mapToGlobal(offsetX, offsetY)
            if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
            {
                const localPoint = overlayItem.mapFromGlobal(globalPoint.x, globalPoint.y)
                if (localPoint && isFinite(localPoint.x) && isFinite(localPoint.y))
                    return localPoint
            }
        }

        const mappedPoint = anchorButton.mapToItem(overlayItem, offsetX, offsetY)
        if (mappedPoint && isFinite(mappedPoint.x) && isFinite(mappedPoint.y))
            return mappedPoint

        return null
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

    modal: false
    focus: true
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    transformOrigin: Item.TopRight

    enter: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 170
                easing.type: Easing.OutCubic
            }

            NumberAnimation
            {
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 170
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 130
                easing.type: Easing.OutCubic
            }

            NumberAnimation
            {
                property: "scale"
                from: 1.0
                to: 0.97
                duration: 130
                easing.type: Easing.OutCubic
            }
        }
    }

    width: Math.max(_panel.implicitWidth, _minPanelWidth)
    height: Math.min(_panel.implicitHeight, _availableHeightFromAnchor)
    onAboutToShow:
    {
        _touchGeometryRevision()
    }
    onOpened:
    {
        Qt.callLater(_touchGeometryRevision)
        _systemResourcesRefreshActive = true
        if (bridge)
            bridge.refreshControlCenterSystemResources()
    }
    onClosed:
    {
        _systemResourcesRefreshActive = false
    }
    onVisibleChanged:
    {
        if (!visible)
            _systemResourcesRefreshActive = false
    }
    anchors.centerIn: undefined
    x:
    {
        const dep = _geometryRevision
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        const minX = _margin
        const maxX = Math.max(minX, overlayItem.width - width - _margin)
        let targetX = maxX

        if (anchorButton)
        {
            const p = _anchorPointInOverlay(anchorButton.width, 0)
            if (p)
                targetX = p.x - width
        }

        return Math.max(minX, Math.min(maxX, targetX))
    }
    y:
    {
        const dep = _geometryRevision
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        const minY = _margin
        return Math.max(minY, _targetY)
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

    background: Rectangle
    {
        color: Qt.alpha(controlCenter._panelColor, 0)
    }

    contentItem: Rectangle
    {
        id: _panel
        implicitWidth: Math.max(controlCenter._minPanelWidth, _panelContent.implicitWidth + (controlCenter._panelInsetX * 2))
        implicitHeight: _panelContent.implicitHeight + (controlCenter._panelInsetY * 2)
        radius: Maui.Style.radiusV
        color: controlCenter._panelColor
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
                                    console.log("ControlCenter DND toggle", checked, "controller=", controlCenter.notificationsControllerRef ? controlCenter.notificationsControllerRef.dndEnabled : "null")
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
