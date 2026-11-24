import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects

import org.mauikit.controls as Maui

Window
{
    id: workspaceMovePopup

    property Item anchorItem
    property QtObject rootWindow
    property QtObject bridge
    property Item overlayItem
    property string windowAddress: ""
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property int _geometryRevision: 0
    property bool _fadeOutPending: false
    property bool _panelOpen: false

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100
    readonly property int _baseUnit: Maui.Style.units.gridUnit
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: Maui.Style.space.small * 2
    readonly property int _panelInsetX: Maui.Style.space.small
    readonly property int _panelInsetY: Maui.Style.space.small
    readonly property color _panelColor: rootWindow ? rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
    readonly property int _preferredPanelWidth: Maui.Handy.isMobile ? _baseUnit * 12 : _baseUnit * 14
    readonly property int _rowHeight: Maui.Style.rowHeight
    readonly property int _availableHeightFromAnchor:
    {
        const screenGeometry = _screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _preferredPanelHeight

        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            targetY = popupTargetY
        else if (anchorItem)
        {
            const point = _anchorPointInScreen(0, 0)
            if (point)
                targetY = point.y + _dropOffset
        }

        return Math.max(0, screenGeometry.height - Math.max(_margin, targetY) - _margin)
    }
    readonly property int _preferredPanelHeight:
        Math.min(Math.max(_rowHeight, bridge ? bridge.workspaceCount * _rowHeight : 0) + (_panelInsetY * 2), _baseUnit * 20)
    readonly property var workspacesModel: bridge ? bridge.workspaceCount : 0

    Maui.Theme.colorSet: Maui.Theme.View

    color: "transparent"
    visible: false
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Shortcut
    {
        sequences: [ StandardKey.Cancel ]
        onActivated: workspaceMovePopup.close()
    }

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
        if (!anchorItem || !anchorItem.mapToGlobal)
            return null

        const point = anchorItem.mapToGlobal(offsetX, offsetY)
        return point && isFinite(point.x) && isFinite(point.y) ? point : null
    }

    function _screenGeometry()
    {
        const screen = rootWindow ? rootWindow.screen : null
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

    function toggleFromAnchor(item, address)
    {
        if (visible && anchorItem === item && windowAddress === address)
        {
            close()
            return
        }

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        if (visible)
            forceClose()

        anchorItem = item
        windowAddress = address
        open()
    }

    function open()
    {
        if (visible)
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
        interval: workspaceMovePopup._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (workspaceMovePopup._fadeOutPending)
            {
                workspaceMovePopup._fadeOutPending = false
                visible = false
            }
        }
    }

    width:
    {
        const screenGeometry = _screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return _preferredPanelWidth

        return Math.min(_preferredPanelWidth, Math.max(0, screenGeometry.width - (_margin * 2)))
    }

    height: Math.min(_workspaceList.contentHeight + (_panelInsetY * 2), _availableHeightFromAnchor)

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
        }
    }

    x:
    {
        const revision = _geometryRevision
        const screenGeometry = _screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        let targetX = _margin
        if (anchorItem)
        {
            const point = _anchorPointInScreen(0, 0)
            if (point)
                targetX = point.x - (width / 2)
        }

        return Math.max(_margin, Math.min(screenGeometry.width - width - _margin, targetX))
    }

    y:
    {
        const revision = _geometryRevision
        const popupTargetY = rootWindow && rootWindow.popupTargetY ? rootWindow.popupTargetY() : null
        if (popupTargetY !== null)
            return Math.max(_margin, popupTargetY)

        const point = _anchorPointInScreen(0, 0)
        return point ? Math.max(_margin, point.y + _dropOffset) : _margin
    }

    Connections
    {
        target: workspaceMovePopup.anchorItem
        function onXChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onYChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onWidthChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onHeightChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onVisibleChanged() { workspaceMovePopup._touchGeometryRevision() }
    }

    Connections
    {
        target: workspaceMovePopup.overlayItem
        function onWidthChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onHeightChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onXChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onYChanged() { workspaceMovePopup._touchGeometryRevision() }
    }

    Connections
    {
        target: workspaceMovePopup.rootWindow
        function onWidthChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onHeightChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onVisibilityChanged() { workspaceMovePopup._touchGeometryRevision() }
        function onWindowStateChanged() { workspaceMovePopup._touchGeometryRevision() }
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

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        opacity: 0
        scale: 0.97
        transformOrigin: Item.Center
        clip: true
        implicitWidth: workspaceMovePopup.width
        implicitHeight: _workspaceList.contentHeight + (workspaceMovePopup._panelInsetY * 2)
        radius: Maui.Style.radiusV + 3
        color: workspaceMovePopup._panelColor
        border.width: Maui.Style.units.gridUnit / 20
        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
        states: [
            State
            {
                name: "open"
                when: workspaceMovePopup._panelOpen
                PropertyChanges { target: _panel; opacity: 1; scale: 1 }
            },
            State
            {
                name: "closed"
                when: !workspaceMovePopup._panelOpen
                PropertyChanges { target: _panel; opacity: 0; scale: 0.97 }
            }
        ]
        transitions: Transition
        {
            reversible: true
            NumberAnimation
            {
                properties: "opacity,scale"
                duration: workspaceMovePopup._panelOpen ? workspaceMovePopup._fadeInDurationMs : workspaceMovePopup._fadeOutDurationMs
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
            anchors.leftMargin: workspaceMovePopup._panelInsetX
            anchors.rightMargin: workspaceMovePopup._panelInsetX
            anchors.topMargin: workspaceMovePopup._panelInsetY
            anchors.bottomMargin: workspaceMovePopup._panelInsetY
            contentWidth: width
            contentHeight: _workspaceList.contentHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            interactive: contentHeight > height

            ListView
            {
                id: _workspaceList
                width: parent.width
                model: workspaceMovePopup.workspacesModel
                clip: true
                height: contentHeight
                spacing: Maui.Style.space.small
                boundsBehavior: Flickable.StopAtBounds
                interactive: false
                implicitHeight: contentHeight

                delegate: Rectangle
                {
                    required property int index
                    width: ListView.view ? ListView.view.width : workspaceMovePopup._preferredPanelWidth
                    height: workspaceMovePopup._rowHeight
                    radius: Maui.Style.radiusV
                    color: index + 1 === workspaceMovePopup.bridge.currentWorkspace
                           ? Qt.alpha(Maui.Theme.highlightColor, 0.22)
                           : hoveredArea.containsMouse ? Qt.alpha(Maui.Theme.hoverColor, 0.7)
                                                       : Maui.Theme.alternateBackgroundColor
                    border.width: Maui.Style.units.gridUnit / 20
                    border.color: index + 1 === workspaceMovePopup.bridge.currentWorkspace
                                  ? Qt.alpha(Maui.Theme.highlightColor, 0.65)
                                  : Qt.alpha(Maui.Theme.textColor, 0.10)

                    MouseArea
                    {
                        id: hoveredArea
                        anchors.fill: parent
                        z: 1
                        hoverEnabled: true
                        enabled: index + 1 !== workspaceMovePopup.bridge.currentWorkspace
                        onClicked:
                        {
                            workspaceMovePopup.bridge.moveWindowToWorkspace(workspaceMovePopup.windowAddress, index + 1)
                            workspaceMovePopup.close()
                        }
                    }

                    RowLayout
                    {
                        anchors.fill: parent
                        anchors.leftMargin: Maui.Style.space.medium
                        anchors.rightMargin: Maui.Style.space.medium
                        anchors.topMargin: Maui.Style.space.tiny
                        anchors.bottomMargin: Maui.Style.space.tiny
                        spacing: Maui.Style.space.small

                        Label
                        {
                            Layout.fillWidth: true
                            text: i18n("Workspace %1", index + 1)
                            color: Maui.Theme.textColor
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        RadioButton
                        {
                            checked: index + 1 === workspaceMovePopup.bridge.currentWorkspace
                            enabled: false
                            text: ""
                            padding: 0
                            implicitWidth: Maui.Style.iconSize + (Maui.Style.space.tiny * 2)
                            implicitHeight: Maui.Style.iconSize + (Maui.Style.space.tiny * 2)
                        }
                    }
                }
            }
        }
    }
}
