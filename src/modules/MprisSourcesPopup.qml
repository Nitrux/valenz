// SPDX-License-Identifier: BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects

import org.mauikit.controls as Maui

Window
{
    id: mprisSourcesPopup

    property Item anchorItem
    property QtObject rootWindow
    property QtObject bridge
    property Item overlayItem
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property int _geometryRevision: 0
    property bool _fadeOutPending: false
    property bool _panelOpen: false

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100

    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInsetX: 6
    readonly property int _panelInsetY: 6
    readonly property color _panelColor: mprisSourcesPopup.rootWindow ? mprisSourcesPopup.rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
    readonly property int _cardPadding: Math.max(Maui.Style.space.medium, Maui.Style.space.small + 2)
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    readonly property int _preferredPanelWidth: Maui.Handy.isMobile ? _baseUnit * 18 : _baseUnit * 22
    readonly property int _minPanelHeight: Maui.Handy.isMobile ? _baseUnit * 12 : _baseUnit * 14
    readonly property int _preferredPanelHeight: Maui.Handy.isMobile ? _baseUnit * 18 : _baseUnit * 20
    readonly property int _rowHeight: Math.max(52, Maui.Style.toolBarHeightAlt)
    readonly property var sourcesModel: bridge ? bridge.mprisSources : []
    readonly property bool _hasSources: sourcesModel && sourcesModel.length > 0

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted: { }
    color: "transparent"

    visible: false
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Shortcut
    {
        sequences: [ StandardKey.Cancel ]
        onActivated: mprisSourcesPopup.close()
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

    function _sourceLabel(source)
    {
        if (!source)
            return ""

        const title = String(source.title || "").trim()
        const artist = String(source.artist || "").trim()
        const subtitle = String(source.subtitle || "").trim()
        if (title.length > 0 && artist.length > 0)
            return title + " - " + artist
        if (title.length > 0)
            return title
        return subtitle.length > 0 ? subtitle : String(source.serviceName || "").trim()
    }

    function _sourceSubLabel(source)
    {
        if (!source)
            return ""

        const backend = String(source.backend || "").trim().toLowerCase()
        const title = String(source.title || "").trim()
        const artist = String(source.artist || "").trim()
        const subtitle = String(source.subtitle || "").trim()
        const serviceName = String(source.serviceName || "").trim()

        if (backend === "bluez")
        {
            if (title.length === 0 && artist.length === 0)
                return i18n("Bluetooth")

            return subtitle.length > 0 ? subtitle : i18n("Bluetooth")
        }

        return subtitle.length > 0 ? subtitle : serviceName.replace(/^org\.mpris\.MediaPlayer2\./, "")
    }

    function _anchorPointInScreen(offsetX, offsetY)
    {
        if (!anchorItem || !anchorItem.mapToGlobal)
        {
            return null
        }

        const globalPoint = anchorItem.mapToGlobal(offsetX, offsetY)
        if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
        {
            return globalPoint
        }
        return null
    }

    function _screenGeometry()
    {
        const screen = mprisSourcesPopup.rootWindow ? mprisSourcesPopup.rootWindow.screen : null
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

    function _availableHeightFromAnchor()
    {
        const screenGeometry = mprisSourcesPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _preferredPanelHeight

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        const startY = Math.max(minY, targetY)
        return Math.max(0, screenGeometry.height - startY - _margin)
    }

    function toggleFromAnchor()
    {
        if (visible)
        {
            close()
            return
        }

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        open()
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
        interval: mprisSourcesPopup._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (mprisSourcesPopup._fadeOutPending)
            {
                mprisSourcesPopup._fadeOutPending = false
                visible = false
            }
        }
    }

    width:
    {
        const screenGeometry = mprisSourcesPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return _preferredPanelWidth

        const available = Math.max(0, screenGeometry.width - (_margin * 2))
        if (available <= 0)
            return _preferredPanelWidth

        return Math.min(_preferredPanelWidth, available)
    }

    height:
    {
        const sourceCount = _cardList ? _cardList.count : 0
        const contentHeight = sourceCount > 0
                ? ((_cardList.contentHeight || 0) + (_panelInsetY * 2))
                : _preferredPanelHeight
        return Math.min(contentHeight, _availableHeightFromAnchor())
    }

    onVisibleChanged:
    {
        if (visible)
        {
            opened()
            _touchGeometryRevision()
            Qt.callLater(_touchGeometryRevision)
            _deferredGeometryRefreshTimer.restart()
            _logPopupGeometry("visible")
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            closed()
            _lastClosedAtMs = Date.now()
        }
    }

    x:
    {
        const dep = _geometryRevision
        const screenGeometry = mprisSourcesPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        let targetX = _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetX = p.x - (width / 2)
        }

        const minX = _margin
        const maxX = Math.max(minX, screenGeometry.width - width - _margin)
        const finalX = Math.max(minX, Math.min(maxX, targetX))
        return finalX
    }

    y:
    {
        const dep = _geometryRevision
        const overlay = mprisSourcesPopup.overlayItem
        if (!overlay)
            return _margin

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorItem)
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
        target: mprisSourcesPopup.anchorItem

        function onXChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onYChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onWidthChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onHeightChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onVisibleChanged() { mprisSourcesPopup._touchGeometryRevision() }
    }

    Connections
    {
        target: mprisSourcesPopup.overlayItem

        function onWidthChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onHeightChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onXChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onYChanged() { mprisSourcesPopup._touchGeometryRevision() }
    }

    Connections
    {
        target: mprisSourcesPopup.rootWindow

        function onWidthChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onHeightChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onVisibilityChanged() { mprisSourcesPopup._touchGeometryRevision() }
        function onWindowStateChanged() { mprisSourcesPopup._touchGeometryRevision() }
    }

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        opacity: 0.0
        scale: 0.97
        transformOrigin: Item.Center
        clip: true
        implicitWidth: mprisSourcesPopup.width
        implicitHeight: _contentColumn.implicitHeight + (mprisSourcesPopup._panelInsetY * 2)
        radius: Maui.Style.radiusV +3
        color: mprisSourcesPopup._panelColor
        border.width: 1
        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
        states: [
            State
            {
                name: "open"
                when: mprisSourcesPopup._panelOpen
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
                when: !mprisSourcesPopup._panelOpen
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
                duration: mprisSourcesPopup._panelOpen ? mprisSourcesPopup._fadeInDurationMs : mprisSourcesPopup._fadeOutDurationMs
                easing.type: Easing.InOutCubic
            }
        }
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: false
            shadowColor: "#80000000"
        }

        Flickable
        {
            id: _contentFlick
            anchors.fill: parent
            anchors.leftMargin: mprisSourcesPopup._panelInsetX
            anchors.rightMargin: mprisSourcesPopup._panelInsetX
            anchors.topMargin: mprisSourcesPopup._panelInsetY
            anchors.bottomMargin: mprisSourcesPopup._panelInsetY
            contentWidth: width
            contentHeight: _contentColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            interactive: contentHeight > height

            ColumnLayout
            {
                id: _contentColumn
                width: _contentFlick.width
                spacing: Maui.Style.space.small

                ListView
                {
                    id: _cardList
                    Layout.fillWidth: true
                    model: mprisSourcesPopup.sourcesModel
                    clip: true
                    spacing: Maui.Style.space.small
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: false
                    implicitHeight: contentHeight

                    delegate: Rectangle
                    {
                        required property var modelData
                        width: ListView.view ? ListView.view.width : mprisSourcesPopup._preferredPanelWidth
                        height: mprisSourcesPopup._rowHeight
                        radius: Maui.Style.radiusV
                        color: modelData.selected ? Qt.alpha(Maui.Theme.highlightColor, 0.22)
                                                  : hoveredArea.containsMouse ? Qt.alpha(Maui.Theme.hoverColor, 0.7)
                                                                              : Maui.Theme.alternateBackgroundColor
                        border.width: modelData.selected ? 1 : 0
                        border.color: modelData.selected ? Qt.alpha(Maui.Theme.highlightColor, 0.65) : "transparent"

                        MouseArea
                        {
                            id: hoveredArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked:
                            {
                                const window = mprisSourcesPopup
                                if (!window || !window.bridge)
                                    return

                                window.bridge.selectMprisSource(modelData.serviceName)
                                window.close()
                            }
                        }

                        RowLayout
                        {
                            anchors.fill: parent
                            anchors.margins: mprisSourcesPopup._cardPadding
                            spacing: Maui.Style.space.small

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 2

                                Label
                                {
                                    Layout.fillWidth: true
                                    text: mprisSourcesPopup._sourceLabel(modelData)
                                    color: Maui.Theme.textColor
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Label
                                {
                                    Layout.fillWidth: true
                                    text: mprisSourcesPopup._sourceSubLabel(modelData)
                                    color: Qt.alpha(Maui.Theme.textColor, 0.78)
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    visible: !mprisSourcesPopup._hasSources
                    radius: Maui.Style.radiusV
                    color: Maui.Theme.alternateBackgroundColor
                    border.color: Qt.alpha(Maui.Theme.textColor, 0.08)
                    opacity: 0.95
                    implicitHeight: 64

                    Label
                    {
                        anchors.centerIn: parent
                        text: i18n("No media sources")
                        color: Maui.Theme.textColor
                        opacity: 0.8
                    }
                }
            }
        }
    }
}
