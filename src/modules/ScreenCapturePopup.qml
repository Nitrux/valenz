import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{
    id: screenCapturePopup

    property Item anchorButton
    property var rootWindow
    property QtObject bridge
    property Item overlayItem
    property bool useSystemThemeIcons: true
    property var glyphForIcon
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property int _geometryRevision: 0
    property bool _fadeOutPending: false
    property bool _panelOpen: false
    property bool recordingActive: false
    property int recordingElapsedSeconds: 0
    property double _recordingStartedAtMs: -1

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100
    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInsetX: 6
    readonly property int _panelInsetY: 6
    readonly property color _panelColor: screenCapturePopup.rootWindow ? screenCapturePopup.rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
    readonly property int _cardPadding: Math.max(Maui.Style.space.medium, Maui.Style.space.small + 2)
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    readonly property string _nerdFontFamily: "Symbols Nerd Font Mono"
    readonly property int _iconSize: 22
    readonly property int _iconSlotSize: screenCapturePopup.useSystemThemeIcons ? 28 : 32
    readonly property real _availableHeightFromAnchor:
    {
        const screenGeometry = screenCapturePopup._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _panel.implicitHeight

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorButton)
        {
            const point = _anchorPointInScreen(0, 0)
            if (point)
                targetY = point.y + Maui.Style.space.small + _dropOffset
        }

        const startY = Math.max(minY, targetY)
        return Math.max(0, screenGeometry.height - startY - _margin)
    }

    Maui.Theme.colorSet: Maui.Theme.View

    color: "transparent"
    visible: false
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Shortcut
    {
        sequences: [ StandardKey.Cancel, "Escape" ]
        context: Qt.ApplicationShortcut
        enabled: screenCapturePopup.visible
        onActivated: screenCapturePopup.close()
    }

    signal closed()

    onClosing: function(closeEvent)
    {
        if (_fadeOutPending)
            return

        closeEvent.accepted = false
        close()
    }

    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _anchorPointInScreen(offsetX, offsetY)
    {
        if (!anchorButton || !anchorButton.mapToGlobal)
            return null

        const point = anchorButton.mapToGlobal(offsetX, offsetY)
        if (point && isFinite(point.x) && isFinite(point.y))
            return point

        return null
    }

    function _screenGeometry()
    {
        const screen = screenCapturePopup.rootWindow ? screenCapturePopup.rootWindow.screen : null
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

    function toggleCapture(captureType, mode)
    {
        const isRecording = captureType === "record"
        const isStopRecording = isRecording && mode === "-stop"

        if (bridge && bridge.launchToma)
            bridge.launchToma(captureType, mode)

        if (isStopRecording)
            setRecordingActive(false)

        close()
    }

    Connections
    {
        target: screenCapturePopup.bridge

        function onTomaRecordingStarted()
        {
            screenCapturePopup.setRecordingActive(true)
        }

        function onTomaRecordingFinished()
        {
            screenCapturePopup.setRecordingActive(false)
        }
    }

    function setRecordingActive(active)
    {
        if (recordingActive === active)
            return

        recordingActive = active
        if (active)
        {
            _recordingStartedAtMs = Date.now()
            recordingElapsedSeconds = 0
            _recordingTimer.restart()
        }
        else
        {
            _recordingStartedAtMs = -1
            recordingElapsedSeconds = 0
            _recordingTimer.stop()
        }
    }

    readonly property string recordingTimeText:
    {
        const totalSeconds = Math.max(0, recordingElapsedSeconds)
        const seconds = String(totalSeconds % 60).padStart(2, "0")
        const minutes = Math.floor(totalSeconds / 60)
        if (minutes < 60)
            return String(minutes) + ":" + seconds

        const hours = Math.floor(minutes / 60)
        return String(hours) + ":" + String(minutes % 60).padStart(2, "0") + ":" + seconds
    }

    Timer
    {
        id: _recordingTimer
        interval: 1000
        repeat: true
        onTriggered:
        {
            if (screenCapturePopup._recordingStartedAtMs < 0)
                return

            screenCapturePopup.recordingElapsedSeconds = Math.floor((Date.now() - screenCapturePopup._recordingStartedAtMs) / 1000)
        }
    }

    function open()
    {
        if (visible)
            return

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        if (rootWindow && rootWindow.closeTransientPopups)
            rootWindow.closeTransientPopups()

        _fadeOutPending = false
        _panelOpen = false
        visible = true
        requestActivate()
        Qt.callLater(function()
        {
            if (visible)
                _panelOpen = true
        })
        _touchGeometryRevision()
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
        id: _fadeOutTimer
        interval: screenCapturePopup._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (screenCapturePopup._fadeOutPending)
            {
                screenCapturePopup._fadeOutPending = false
                visible = false
            }
        }
    }

    Timer
    {
        id: _deferredGeometryRefreshTimer
        interval: 32
        repeat: false
        onTriggered:
        {
            if (visible)
                _touchGeometryRevision()
        }
    }

    onVisibleChanged:
    {
        if (visible)
        {
            _touchGeometryRevision()
            Qt.callLater(_touchGeometryRevision)
            _deferredGeometryRefreshTimer.restart()
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            _lastClosedAtMs = Date.now()
            closed()
        }
    }

    width: Math.max(_panel.implicitWidth, _minPanelWidth)
    height: Math.min(_panel.implicitHeight, _availableHeightFromAnchor)

    x:
    {
        const dependency = _geometryRevision
        const screenGeometry = screenCapturePopup._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        const minX = _margin
        const maxX = Math.max(minX, screenGeometry.width - width - _margin)
        let targetX = maxX
        if (anchorButton)
        {
            const point = _anchorPointInScreen(0, 0)
            if (point)
                targetX = point.x - width
        }

        return Math.max(minX, Math.min(maxX, targetX))
    }

    y:
    {
        const dependency = _geometryRevision
        const overlay = screenCapturePopup.overlayItem
        if (!overlay)
            return 0

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorButton)
        {
            const point = _anchorPointInScreen(0, 0)
            if (point)
                targetY = point.y + Maui.Style.space.small + _dropOffset
        }

        const finalY = Math.max(minY, targetY)
        return finalY
    }

    Connections
    {
        target: screenCapturePopup.anchorButton
        function onXChanged() { screenCapturePopup._touchGeometryRevision() }
        function onYChanged() { screenCapturePopup._touchGeometryRevision() }
        function onWidthChanged() { screenCapturePopup._touchGeometryRevision() }
        function onHeightChanged() { screenCapturePopup._touchGeometryRevision() }
        function onVisibleChanged() { screenCapturePopup._touchGeometryRevision() }
    }

    Connections
    {
        target: screenCapturePopup.overlayItem
        function onWidthChanged() { screenCapturePopup._touchGeometryRevision() }
        function onHeightChanged() { screenCapturePopup._touchGeometryRevision() }
        function onXChanged() { screenCapturePopup._touchGeometryRevision() }
        function onYChanged() { screenCapturePopup._touchGeometryRevision() }
    }

    Connections
    {
        target: screenCapturePopup.rootWindow
        function onWidthChanged() { screenCapturePopup._touchGeometryRevision() }
        function onHeightChanged() { screenCapturePopup._touchGeometryRevision() }
        function onVisibilityChanged() { screenCapturePopup._touchGeometryRevision() }
        function onWindowStateChanged() { screenCapturePopup._touchGeometryRevision() }
    }

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        implicitWidth: Math.max(screenCapturePopup._minPanelWidth, _panelContent.implicitWidth + (screenCapturePopup._panelInsetX * 2))
        implicitHeight: _panelContent.implicitHeight + (screenCapturePopup._panelInsetY * 2)
        opacity: 0.0
        scale: 0.97
        transformOrigin: Item.Center
        radius: Maui.Style.radiusV + 3
        color: screenCapturePopup._panelColor
        border.width: 1
        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
        states: [
            State
            {
                name: "open"
                when: screenCapturePopup._panelOpen
                PropertyChanges { target: _panel; opacity: 1.0; scale: 1.0 }
            },
            State
            {
                name: "closed"
                when: !screenCapturePopup._panelOpen
                PropertyChanges { target: _panel; opacity: 0.0; scale: 0.97 }
            }
        ]
        transitions: Transition
        {
            reversible: true
            NumberAnimation
            {
                properties: "opacity,scale"
                duration: screenCapturePopup._panelOpen ? screenCapturePopup._fadeInDurationMs : screenCapturePopup._fadeOutDurationMs
                easing.type: Easing.InOutCubic
            }
        }
        layer.enabled: visible && GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: false
            shadowColor: "#80000000"
        }

        Flickable
        {
            anchors.fill: parent
            anchors.leftMargin: screenCapturePopup._panelInsetX
            anchors.rightMargin: screenCapturePopup._panelInsetX
            anchors.topMargin: screenCapturePopup._panelInsetY
            anchors.bottomMargin: screenCapturePopup._panelInsetY
            contentWidth: width
            contentHeight: _panelContent.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ColumnLayout
            {
                id: _panelContent
                width: parent.width
                spacing: Maui.Style.space.small

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    clip: true
                    padding: screenCapturePopup._cardPadding
                    text: ""
                    label2.text: ""
                    template.visible: false
                    background: Rectangle
                    {
                        color: Maui.Theme.alternateBackgroundColor
                        radius: Maui.Style.radiusV
                        border.width: 1
                        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width
                        spacing: Maui.Style.space.medium

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small


                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: i18n("Screenshots"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: i18n("Capture your screen as an image."); color: Maui.Theme.disabledTextColor; wrapMode: Text.WordWrap }
                            }
                        }

                        Maui.ToolActions
                        {
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            display: ToolButton.IconOnly
                            checkable: false
                            autoExclusive: false
                            expanded: true

                            Action { icon.name: "screenshot-fullscreen"; text: i18n("Capture full screen"); onTriggered: screenCapturePopup.toggleCapture("screenshot", "-f") }
                            Action { icon.name: "screenshot-select"; text: i18n("Capture region"); onTriggered: screenCapturePopup.toggleCapture("screenshot", "-s") }
                            Action { icon.name: "screenshot-window"; text: i18n("Capture window"); onTriggered: screenCapturePopup.toggleCapture("screenshot", "-w") }
                        }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    clip: true
                    padding: screenCapturePopup._cardPadding
                    text: ""
                    label2.text: ""
                    template.visible: false
                    background: Rectangle
                    {
                        color: Maui.Theme.alternateBackgroundColor
                        radius: Maui.Style.radiusV
                        border.width: 1
                        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width
                        spacing: Maui.Style.space.medium

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: Maui.Style.space.small


                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: i18n("Screen recording"); color: Maui.Theme.textColor; font.weight: Font.DemiBold }
                                Label { text: i18n("Record your screen as a video."); color: Maui.Theme.disabledTextColor; wrapMode: Text.WordWrap }
                            }
                        }

                        Maui.ToolActions
                        {
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            display: ToolButton.IconOnly
                            checkable: false
                            autoExclusive: false
                            expanded: true

                            Action { icon.name: "screen-record-fullscreen"; text: i18n("Record full screen"); onTriggered: screenCapturePopup.toggleCapture("record", "-f") }
                            Action { icon.name: "screen-record-select"; text: i18n("Record region"); onTriggered: screenCapturePopup.toggleCapture("record", "-s") }
                            Action { icon.name: "screen-record-window"; text: i18n("Record window"); onTriggered: screenCapturePopup.toggleCapture("record", "-w") }
                        }
                    }
                }
            }
        }
    }
}
