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
    property bool useSystemThemeIcons: true
    property var glyphForIcon
    property var rootWindow
    property QtObject bridge
    property QtObject notificationsControllerRef
    property Item overlayItem

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted: { }
    color: "transparent"

    visible: false
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Shortcut
    {
        sequences: [ StandardKey.Cancel, "Escape" ]
        context: Qt.ApplicationShortcut
        enabled: controlCenter.visible
        onActivated: controlCenter.close()
    }

    Item
    {
        anchors.fill: parent
        focus: controlCenter.visible
        Keys.onEscapePressed: function(event)
        {
            event.accepted = true
            controlCenter.close()
        }
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
    readonly property int _panelInsetX: 6
    readonly property int _panelInsetY: 6
    readonly property color _panelColor: controlCenter.rootWindow ? controlCenter.rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
    readonly property string _nerdFontFamily: "Symbols Nerd Font Mono"
    readonly property int _glyphSize: Math.max(15, Math.round(Maui.Style.iconSizes.medium * 0.9))
    readonly property int _iconSize: 16
    readonly property int _fontIconSize: 22
    readonly property int _iconSlotSize: controlCenter.useSystemThemeIcons ? _iconSize : _fontIconSize
    readonly property int _cardPadding: Maui.Style.space.small
    readonly property int _sliderBadgeWidth: 48
    readonly property color _volumeBarColor: (controlCenter.bridge ? controlCenter.bridge.controlCenterVolumeMuted : false) ? Maui.Theme.disabledTextColor : Maui.Theme.highlightColor
    readonly property color _volumeIndicatorColor: (controlCenter.bridge ? controlCenter.bridge.controlCenterVolumeMuted : false) ? Maui.Theme.disabledTextColor : Maui.Theme.textColor
    readonly property color _networkDownloadColor: Maui.Theme.positiveBackgroundColor
    readonly property string _microphoneVolumePercentage: controlCenter.bridge ? controlCenter.bridge.controlCenterMicrophoneVolumePercentage : "0%"
    readonly property int _microphonePercentage:
    {
        const text = controlCenter._microphoneVolumePercentage
        const parsed = parseInt(String(text || "").trim(), 10)
        return isNaN(parsed) ? 0 : Math.max(0, Math.min(100, parsed))
    }
    readonly property bool _microphoneAvailable: controlCenter.bridge ? controlCenter.bridge.controlCenterMicrophoneAvailable : false
    readonly property int _toolActionMinSize: Math.max(28, Maui.Style.toolBarHeightAlt)
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    property bool _cellularEnabled: false
    readonly property bool _dndEnabled: controlCenter.notificationsControllerRef ? controlCenter.notificationsControllerRef.dndEnabled : false
    readonly property bool _nightLightEnabled: controlCenter.bridge ? controlCenter.bridge.controlCenterNightLightEnabled : false
    readonly property bool _nightLightAvailable: controlCenter.bridge ? controlCenter.bridge.controlCenterNightLightAvailable : false
    readonly property bool _darkMode: controlCenter.bridge ? controlCenter.bridge.darkMode : false
    readonly property bool _darkModeAvailable: controlCenter.bridge ? controlCenter.bridge.darkModeAvailable : false
    readonly property bool _cameraEnabled: controlCenter.bridge ? controlCenter.bridge.cameraEnabled : false
    readonly property bool _cameraAvailable: controlCenter.bridge ? controlCenter.bridge.cameraAvailable : false
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
    readonly property string _networkIconName:
    {
        const state = controlCenter.bridge ? String(controlCenter.bridge.controlCenterNetworkState || "offline").toLowerCase() : "offline"
        if (state === "wired")
            return "network-wired"
        if (state === "wireless")
            return "network-wireless"
        return "network-disconnect"
    }
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
    property int _microphonePendingPercentage: -1

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100
    readonly property real _targetY:
    {
        const overlayItem = controlCenter.overlayItem
        if (!overlayItem)
            return 0

        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorButton)
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
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorButton)
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
            case "power-saver": return i18n("Power Saver")
            case "balanced": return i18n("Balanced")
            case "performance": return i18n("Performance")
            default: return profileName
        }
    }

    function _networkModeTitle()
    {
        return i18n("WiFi")
    }

    function _networkModeSubtitle()
    {
        const hasWirelessAdapter = controlCenter.bridge ? controlCenter.bridge.controlCenterWirelessAvailable : false
        const wirelessEnabled = hasWirelessAdapter && controlCenter.bridge.controlCenterWirelessEnabled
        const isWirelessConnected = wirelessEnabled && controlCenter.bridge.controlCenterWirelessConnected
        const isWiredConnected = controlCenter.bridge ? controlCenter.bridge.controlCenterWiredConnected : false

        if (!hasWirelessAdapter)
            return i18n("Not Available")
        if (!wirelessEnabled)
            return i18n("Off")
        if (isWirelessConnected && isWiredConnected)
            return i18n("WiFi On - Wired Connected")
        if (isWirelessConnected)
            return i18n("Connected")
        if (isWiredConnected)
            return i18n("Disconnected")
        return i18n("On")
    }

    function _commitVolumeFromSlider()
    {
        if (controlCenter.bridge)
            controlCenter.bridge.setControlCenterVolumePercentageFromSlider(Math.round(_volumeSlider.value))
    }

    function _commitMicrophoneVolumeFromSlider()
    {
        if (controlCenter.bridge && controlCenter._microphoneAvailable)
            controlCenter.bridge.setControlCenterMicrophoneVolumePercentageFromSlider(Math.round(_microphoneSlider.value))
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

    function _networkRateText(bytesPerSecond)
    {
        const value = Math.max(0, Number(bytesPerSecond) || 0)
        if (value < 1000)
            return Math.round(value) + " B/s"
        if (value < 1000000)
            return (value / 1000).toFixed(1) + " kB/s"
        if (value < 1000000000)
            return (value / 1000000).toFixed(1) + " MB/s"
        return (value / 1000000000).toFixed(1) + " GB/s"
    }

    property var _networkUploadHistory: []
    property var _networkDownloadHistory: []
    property real _networkAnimatedUploadRate: 0
    property real _networkAnimatedDownloadRate: 0
    property real _networkUploadStartRate: 0
    property real _networkDownloadStartRate: 0
    property real _networkUploadTargetRate: 0
    property real _networkDownloadTargetRate: 0
    property double _networkAnimationStartedAt: 0
    readonly property int _networkHistoryLength: 60
    readonly property real _networkHistoryMaximum: Math.max(1, ..._networkUploadHistory, ..._networkDownloadHistory)

    function _temperatureText(value)
    {
        return value >= 0 ? value + " °C" : i18n("N/A")
    }

    Connections
    {
        target: controlCenter.bridge
        function onControlCenterNetworkSampled()
        {
            if (!controlCenter._systemResourcesRefreshActive)
                return
            const now = Date.now()
            const uploadRate = Math.max(0, controlCenter.bridge.controlCenterNetworkUploadRate)
            const downloadRate = Math.max(0, controlCenter.bridge.controlCenterNetworkDownloadRate)
            controlCenter._networkUploadStartRate = controlCenter._networkAnimatedUploadRate
            controlCenter._networkDownloadStartRate = controlCenter._networkAnimatedDownloadRate
            controlCenter._networkUploadTargetRate = uploadRate
            controlCenter._networkDownloadTargetRate = downloadRate
            controlCenter._networkAnimationStartedAt = now
            controlCenter._networkUploadHistory = controlCenter._networkUploadHistory.concat([uploadRate]).slice(-controlCenter._networkHistoryLength)
            controlCenter._networkDownloadHistory = controlCenter._networkDownloadHistory.concat([downloadRate]).slice(-controlCenter._networkHistoryLength)
        }
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
        id: _microphoneApplyTimer
        interval: 150
        repeat: false
        onTriggered:
        {
            if (_microphoneSlider._applyPending)
            {
                _microphoneSlider._applyPending = false
                controlCenter._commitMicrophoneVolumeFromSlider()
            }
        }
    }

    Timer
    {
        interval: 16
        repeat: true
        running: controlCenter._systemResourcesRefreshActive
        onTriggered:
        {
            const progress = Math.min(1, Math.max(0, (Date.now() - controlCenter._networkAnimationStartedAt) / 1000))
            const easedProgress = progress * progress * (3 - 2 * progress)
            controlCenter._networkAnimatedUploadRate = controlCenter._networkUploadStartRate + (controlCenter._networkUploadTargetRate - controlCenter._networkUploadStartRate) * easedProgress
            controlCenter._networkAnimatedDownloadRate = controlCenter._networkDownloadStartRate + (controlCenter._networkDownloadTargetRate - controlCenter._networkDownloadStartRate) * easedProgress
            if (_networkGraph)
                _networkGraph.requestPaint()
        }
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

        if (rootWindow && rootWindow.closeTransientPopups)
            rootWindow.closeTransientPopups()
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
    }

    function close()
    {
        if (!visible)
            return

        _fadeOutPending = true
        _panelOpen = false
        _fadeOutTimer.restart()
    }

    function forceClose()
    {
        _fadeOutTimer.stop()
        _fadeOutPending = false
        _panelOpen = false
        visible = false
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
            }
        }
    }

    Timer
    {
        id: _initialDataRefreshTimer
        interval: 0
        repeat: false
        onTriggered:
        {
            if (visible && bridge)
            {
                bridge.refreshControlCenterSystemResources()
                bridge.refreshControlCenterRuntimeState()
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
            _systemResourcesRefreshActive = true
            _networkAnimatedUploadRate = bridge ? Math.max(0, bridge.controlCenterNetworkUploadRate) : 0
            _networkAnimatedDownloadRate = bridge ? Math.max(0, bridge.controlCenterNetworkDownloadRate) : 0
            _networkUploadStartRate = _networkAnimatedUploadRate
            _networkDownloadStartRate = _networkAnimatedDownloadRate
            _networkUploadTargetRate = _networkAnimatedUploadRate
            _networkDownloadTargetRate = _networkAnimatedDownloadRate
            _networkAnimationStartedAt = Date.now()
            if (bridge)
            {
                bridge.setControlCenterGpuPowerLease(true)
                bridge.setControlCenterRuntimeActive(true)
                _initialDataRefreshTimer.restart()
            }
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            closed()
            _networkUploadHistory = []
            _networkDownloadHistory = []
            _systemResourcesRefreshActive = false
            _initialDataRefreshTimer.stop()
            if (bridge)
            {
                bridge.setControlCenterGpuPowerLease(false)
                bridge.setControlCenterRuntimeActive(false)
            }
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
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorButton)
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
        radius: Maui.Style.radiusV + 3
        color: controlCenter._panelColor
        border.width: 1
        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
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
        layer.enabled: controlCenter._cameraAvailable && GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: false
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
                    background: Rectangle
                    {
                        color: Maui.Theme.alternateBackgroundColor
                        radius: Maui.Style.radiusV
                        border.width: 1
                        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                    }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Item
                            {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                implicitWidth: 36
                                implicitHeight: 36

                                Rectangle
                                {
                                    anchors.fill: parent
                                    radius: width / 2
                                    color: Maui.Theme.alternateBackgroundColor
                                    border.width: 1
                                    border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                                }

                                Maui.IconItem
                                {
                                    anchors.fill: parent
                                    visible: controlCenter.bridge && controlCenter.bridge.userAvatarUrl.length > 0
                                    imageSource: controlCenter.bridge ? controlCenter.bridge.userAvatarUrl : ""
                                    imageSizeHint: 36
                                    imageWidth: 72
                                    imageHeight: 72
                                    maskRadius: 18
                                    fillMode: Image.PreserveAspectCrop
                                }

                                Maui.IconLabel
                                {
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    Layout.minimumHeight: controlCenter._iconSlotSize
                                    Layout.preferredHeight: controlCenter._iconSlotSize
                                    Layout.maximumHeight: controlCenter._iconSlotSize
                                    anchors.centerIn: parent
                                    visible: !(controlCenter.bridge && controlCenter.bridge.userAvatarUrl.length > 0)
                                    width: controlCenter._iconSlotSize
                                    height: controlCenter._iconSlotSize
                                    icon: ({name: controlCenter.useSystemThemeIcons ? "user-identity" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                    text: "\uf007"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
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
                                icon.name: "settings-configure"
                                display: ToolButton.IconOnly

                                onClicked:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.executeControlCenterSettingsCommand()
                                }
                            }

                            Maui.Separator
                            {
                                Layout.preferredHeight: 8
                                height: 8
                            }

                            ToolButton
                            {
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                Layout.minimumHeight: 28
                                text: i18n("Shutdown")
                                display: ToolButton.TextBesideIcon
                                Maui.Theme.textColor: "#cdd6f4"
                                Maui.Theme.highlightedTextColor: "#cdd6f4"

                                background: Rectangle
                                {
                                    radius: Maui.Style.radiusV
                                    color: parent.down ? Qt.darker(Maui.Theme.negativeBackgroundColor, 1.12) : Maui.Theme.negativeBackgroundColor
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
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        visible: true
                        enabled: controlCenter.bridge ? controlCenter.bridge.controlCenterWirelessAvailable : false
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "network-wireless" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf1eb"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
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
                                checked: controlCenter.bridge
                                         ? controlCenter.bridge.controlCenterWirelessAvailable
                                           && controlCenter.bridge.controlCenterWirelessEnabled
                                         : false
                                onToggled:
                                {
                                    if (!controlCenter.bridge)
                                        return

                                    controlCenter.bridge.controlCenterWirelessEnabled = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        visible: true
                        enabled: controlCenter.bridge ? controlCenter.bridge.controlCenterBluetoothAvailable : false
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "bluetooth-active" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf294"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0

                                Label
                                {
                                    text: i18n("Bluetooth")
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
                                checked: controlCenter.bridge ? (controlCenter.bridge.controlCenterBluetoothAvailable && controlCenter.bridge.controlCenterBluetoothEnabled) : false
                                onToggled:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.controlCenterBluetoothEnabled = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        visible: true
                        enabled: controlCenter._darkModeAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "dark-theme-mode" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf042"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: i18n("Dark mode"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._darkMode ? i18n("On") : i18n("Off"); color: Maui.Theme.disabledTextColor }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter._darkMode
                                enabled: controlCenter._darkModeAvailable
                                onToggled:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.darkMode = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        visible: true
                        enabled: controlCenter._cameraAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "camera-web" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf030"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: i18n("Camera"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._cameraAvailable ? (controlCenter._cameraEnabled ? i18n("On") : i18n("Off")) : i18n("Unavailable"); color: Maui.Theme.disabledTextColor }
                            }

                            Item { Layout.fillWidth: true }

                            Switch
                            {
                                checked: controlCenter._cameraEnabled
                                enabled: controlCenter._cameraAvailable
                                onToggled:
                                {
                                    if (controlCenter.bridge)
                                        controlCenter.bridge.cameraEnabled = checked
                                }
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        visible: true
                        enabled: controlCenter._nightLightAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "night-light" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf186"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: i18n("Night Light"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
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
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "notifications-disabled" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf05e"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                spacing: 0
                                Label { text: i18n("Do Not Disturb"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
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
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        enabled: controlCenter.bridge ? !controlCenter.bridge.controlCenterPowerProfileAutomatic : false
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? controlCenter._powerProfileIconName : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: controlCenter._powerProfileIconName === "power-profile-performance" ? "\uf0e7" : (controlCenter._powerProfileIconName === "power-profile-power-saver" ? "\uf06c" : "\ueeb2")
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                spacing: 0

                                Label
                                {
                                    text: i18n("Power Profile")
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
                                readonly property string _fallbackLabel: i18n("Select Profile")
                                readonly property string _automaticLabel: i18n("Automatic")
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

                                    if (_automaticLabel.length > longest.length)
                                        longest = _automaticLabel

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
                                displayText: controlCenter.bridge && controlCenter.bridge.controlCenterPowerProfileAutomatic
                                    ? _automaticLabel
                                    : currentIndex === -1 ? _fallbackLabel : controlCenter._powerProfileLabel(currentText)
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
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small
                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "audio-volume-high" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: controlCenter._volumeIndicatorColor})
                                text: "\uf028"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: controlCenter._volumeIndicatorColor
                            }
                            Slider
                            {
                                id: _volumeSlider
                                Layout.fillWidth: true
                                enabled: !!controlCenter.bridge
                                Maui.Theme.highlightColor: controlCenter._volumeBarColor
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
                            WorkspaceBadge
                            {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: controlCenter._sliderBadgeWidth
                                Layout.preferredWidth: controlCenter._sliderBadgeWidth
                                Layout.maximumWidth: controlCenter._sliderBadgeWidth
                                badgeText: controlCenter.bridge ? String(controlCenter._volumePercentage) + "%" : "N/A"
                                bridge: null
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        enabled: controlCenter._microphoneAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small

                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "audio-input-microphone" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf130"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                color: Maui.Theme.textColor
                            }

                            Slider
                            {
                                id: _microphoneSlider
                                property bool _applyPending: false
                                Layout.fillWidth: true
                                enabled: controlCenter._microphoneAvailable
                                from: 0
                                to: 100
                                value: controlCenter._microphonePercentage
                                onMoved:
                                {
                                    controlCenter._microphonePendingPercentage = Math.round(value)
                                    _applyPending = true
                                    _microphoneApplyTimer.restart()
                                }
                                onPressedChanged:
                                {
                                    if (!pressed && _applyPending)
                                    {
                                        _microphoneApplyTimer.stop()
                                        _applyPending = false
                                        controlCenter._commitMicrophoneVolumeFromSlider()
                                    }
                                }
                                Connections
                                {
                                    target: controlCenter.bridge
                                    enabled: !!controlCenter.bridge
                                    function onControlCenterMicrophoneVolumePercentageChanged()
                                    {
                                        if (_microphoneSlider.pressed)
                                            return

                                        if (controlCenter._microphonePendingPercentage >= 0)
                                        {
                                            if (controlCenter._microphonePercentage !== controlCenter._microphonePendingPercentage)
                                                return
                                            controlCenter._microphonePendingPercentage = -1
                                        }

                                        _microphoneSlider.value = controlCenter._microphonePercentage
                                    }
                                    function onControlCenterMicrophoneAvailableChanged()
                                    {
                                        if (!_microphoneSlider.pressed)
                                            _microphoneSlider.value = controlCenter._microphonePercentage
                                    }
                                }
                            }

                            WorkspaceBadge
                            {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: controlCenter._sliderBadgeWidth
                                Layout.preferredWidth: controlCenter._sliderBadgeWidth
                                Layout.maximumWidth: controlCenter._sliderBadgeWidth
                                badgeText: controlCenter._microphoneAvailable ? String(controlCenter._microphonePendingPercentage >= 0 ? controlCenter._microphonePendingPercentage : controlCenter._microphonePercentage) + "%" : "N/A"
                                visible: true
                                bridge: null
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        text: ""
                        label2.text: ""
                        template.visible: false
                        enabled: controlCenter._brightnessAvailable
                        opacity: enabled ? 1.0 : 0.55

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small
                            Maui.IconLabel
                            {
                                Layout.minimumWidth: controlCenter._iconSlotSize
                                Layout.preferredWidth: controlCenter._iconSlotSize
                                Layout.maximumWidth: controlCenter._iconSlotSize
                                Layout.minimumHeight: controlCenter._iconSlotSize
                                Layout.preferredHeight: controlCenter._iconSlotSize
                                Layout.maximumHeight: controlCenter._iconSlotSize
                                icon: ({name: controlCenter.useSystemThemeIcons ? "display-brightness" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                text: "\uf185"
                                font.family: controlCenter._nerdFontFamily
                                font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
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
                            WorkspaceBadge
                            {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: controlCenter._sliderBadgeWidth
                                Layout.preferredWidth: controlCenter._sliderBadgeWidth
                                Layout.maximumWidth: controlCenter._sliderBadgeWidth
                                badgeText: controlCenter._brightnessAvailable ? String(controlCenter._brightnessPercentage) + "%" : "N/A"
                                visible: true
                                bridge: null
                            }
                        }
                    }

                    Maui.SectionItem
                    {
                        background: Rectangle
                        {
                            color: Maui.Theme.alternateBackgroundColor
                            radius: Maui.Style.radiusV
                            border.width: 1
                            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                        }
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        flat: false
                        clip: true
                        padding: controlCenter._cardPadding
                        topPadding: controlCenter._cardPadding + Maui.Style.space.medium
                        bottomPadding: controlCenter._cardPadding + Maui.Style.space.medium
                        text: ""
                        label2.text: ""
                        template.visible: false

                        ColumnLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.medium

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Maui.IconLabel
                                {
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    Layout.minimumHeight: controlCenter._iconSlotSize
                                    Layout.preferredHeight: controlCenter._iconSlotSize
                                    Layout.maximumHeight: controlCenter._iconSlotSize
                                    icon: ({name: controlCenter.useSystemThemeIcons ? "cpu" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                    text: "\uf2db"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                    color: Maui.Theme.textColor
                                }
                                Label { text: i18n("CPU"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._controlCenterCpuPercentage() + "% • " + controlCenter._temperatureText(controlCenter.bridge ? controlCenter.bridge.controlCenterCpuTemperature : -1); color: Maui.Theme.disabledTextColor }
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
                                Maui.IconLabel
                                {
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    Layout.minimumHeight: controlCenter._iconSlotSize
                                    Layout.preferredHeight: controlCenter._iconSlotSize
                                    Layout.maximumHeight: controlCenter._iconSlotSize
                                    icon: ({name: controlCenter.useSystemThemeIcons ? "device-pci-gpu" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                    text: "\uf26c"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                    color: Maui.Theme.textColor
                                }
                                Label { text: i18n("GPU"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: (controlCenter.bridge && controlCenter.bridge.controlCenterGpuPercentage >= 0 ? controlCenter.bridge.controlCenterGpuPercentage + "%" : i18n("N/A")) + " • " + controlCenter._temperatureText(controlCenter.bridge ? controlCenter.bridge.controlCenterGpuTemperature : -1); color: Maui.Theme.disabledTextColor }
                            }
                            ProgressBar
                            {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter.bridge ? Math.max(0, controlCenter.bridge.controlCenterGpuPercentage) : 0
                                enabled: controlCenter.bridge && controlCenter.bridge.controlCenterGpuPercentage >= 0
                            }

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Maui.IconLabel
                                {
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    Layout.minimumHeight: controlCenter._iconSlotSize
                                    Layout.preferredHeight: controlCenter._iconSlotSize
                                    Layout.maximumHeight: controlCenter._iconSlotSize
                                    icon: ({name: controlCenter.useSystemThemeIcons ? "memory" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                    text: "\uefc5"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                    color: Maui.Theme.textColor
                                }
                                Label { text: i18n("RAM"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
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
                                Maui.IconLabel
                                {
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    Layout.minimumHeight: controlCenter._iconSlotSize
                                    Layout.preferredHeight: controlCenter._iconSlotSize
                                    Layout.maximumHeight: controlCenter._iconSlotSize
                                    icon: ({name: controlCenter.useSystemThemeIcons ? "drive-harddisk" : "", source: "", width: controlCenter._iconSlotSize, height: controlCenter._iconSlotSize, color: Maui.Theme.textColor})
                                    text: "\uf0a0"
                                    font.family: controlCenter._nerdFontFamily
                                    font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                    color: Maui.Theme.textColor
                                }
                                Label { text: i18n("Disk"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: controlCenter._controlCenterDiskUsageText(); color: Maui.Theme.disabledTextColor }
                            }
                            ProgressBar
                            {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: controlCenter._controlCenterDiskUsagePercentage()
                            }

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: Maui.Style.space.small
                                Item
                                {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.minimumWidth: controlCenter._iconSlotSize
                                    Layout.preferredWidth: controlCenter._iconSlotSize
                                    Layout.maximumWidth: controlCenter._iconSlotSize
                                    height: 20
                                    Maui.Icon
                                    {
                                        id: _networkResourceIcon
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                        source: controlCenter._networkIconName
                                        color: Maui.Theme.textColor
                                        visible: controlCenter.useSystemThemeIcons && valid
                                    }
                                    Label
                                    {
                                        anchors.centerIn: parent
                                        visible: !controlCenter.useSystemThemeIcons || !_networkResourceIcon.valid
                                        text: controlCenter.glyphForIcon ? controlCenter.glyphForIcon(controlCenter._networkIconName) : (controlCenter._networkIconName === "network-wired" ? "\uf0e8" : (controlCenter._networkIconName === "network-wireless" ? "\uf1eb" : "\uf127"))
                                        font.family: controlCenter._nerdFontFamily
                                        font.pointSize: Math.max(9, Math.round(controlCenter._glyphSize * 0.75))
                                        color: Maui.Theme.textColor
                                    }
                                }
                                Label { text: i18n("Network"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                RowLayout
                                {
                                    spacing: Maui.Style.space.tiny
                                    Label
                                    {
                                        text: "↑"
                                        color: Qt.rgba(Maui.Theme.highlightColor.r, Maui.Theme.highlightColor.g, Maui.Theme.highlightColor.b, 1)
                                    }
                                    Label
                                    {
                                        text: controlCenter._networkRateText(controlCenter._networkAnimatedUploadRate)
                                        color: Maui.Theme.disabledTextColor
                                    }
                                    Label { text: "•"; color: Maui.Theme.disabledTextColor }
                                    Label
                                    {
                                        text: "↓"
                                        color: Qt.rgba(controlCenter._networkDownloadColor.r, controlCenter._networkDownloadColor.g, controlCenter._networkDownloadColor.b, 1)
                                    }
                                    Label
                                    {
                                        text: controlCenter._networkRateText(controlCenter._networkAnimatedDownloadRate)
                                        color: Maui.Theme.disabledTextColor
                                    }
                                }
                            }
                            Canvas
                            {
                                id: _networkGraph
                                Layout.fillWidth: true
                                Layout.preferredHeight: Maui.Style.units.gridUnit * 2
                                readonly property real maximum: controlCenter._networkHistoryMaximum
                                onMaximumChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onPaint:
                                {
                                    const context = getContext("2d")
                                    context.clearRect(0, 0, width, height)
                                    const sampleSpacing = width / (controlCenter._networkHistoryLength - 1)
                                    context.globalAlpha = 0.55

                                    function drawSeries(samples, animatedRate, barColor)
                                    {
                                        if (samples.length === 0)
                                            return
                                        const offset = controlCenter._networkHistoryLength - samples.length
                                        context.fillStyle = barColor
                                        context.beginPath()
                                        context.moveTo(offset * sampleSpacing, height)
                                        for (let i = 0; i < samples.length; ++i)
                                        {
                                            const value = i === samples.length - 1 ? animatedRate : samples[i]
                                            context.lineTo((offset + i) * sampleSpacing, height * (1 - value / maximum))
                                        }
                                        context.lineTo(width, height)
                                        context.closePath()
                                        context.fill()
                                    }

                                    drawSeries(controlCenter._networkUploadHistory, controlCenter._networkAnimatedUploadRate, Maui.Theme.highlightColor)
                                    drawSeries(controlCenter._networkDownloadHistory, controlCenter._networkAnimatedDownloadRate, controlCenter._networkDownloadColor)
                                    context.globalAlpha = 1
                                }
                            }
                        }
                }
            }
        }
        }
    }
}
