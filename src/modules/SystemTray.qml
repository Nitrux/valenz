import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects

import org.mauikit.controls as Maui

RowLayout
{
    id: systemTray

    property QtObject controller
    property QtObject rootWindow
    property bool debugDetails: false
    property int trayMenuIndex: -1
    property bool trayMenuItemIsMenu: false
    property var trayMenuItems: []
    property Item trayMenuAnchor: null

    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _panelInset: Math.max(Maui.Style.space.small, 6)
    readonly property int _preferredMenuWidth: Maui.Handy.isMobile ? 320 : 360
    readonly property int _minimumMenuWidth: Maui.Handy.isMobile ? 280 : 320
    readonly property int _rowHeight: Math.max(40, Maui.Style.toolBarHeightAlt)
    property int _geometryRevision: 0
    property double _lastMenuClosedAtMs: -1
    property bool _menuOpen: false
    property real _trayMenuOriginX: 0
    property real _trayMenuOriginY: 0

    spacing: Maui.Style.space.tiny
    visible: !!controller && controller.count > 0

    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _screenGeometry()
    {
        const screen = (systemTray.rootWindow && systemTray.rootWindow.screen)
                     ? systemTray.rootWindow.screen
                     : (systemTray.Window.window ? systemTray.Window.window.screen : null)
        if (screen && screen.availableGeometry && screen.availableGeometry.width > 0 && screen.availableGeometry.height > 0)
            return screen.availableGeometry
        if (screen && screen.geometry && screen.geometry.width > 0 && screen.geometry.height > 0)
            return screen.geometry
        if (screen && screen.virtualGeometry && screen.virtualGeometry.width > 0 && screen.virtualGeometry.height > 0)
            return screen.virtualGeometry
        if (screen && screen.width > 0 && screen.height > 0)
            return Qt.rect(0, 0, screen.width, screen.height)
        return Qt.rect(0, 0, 0, 0)
    }

    function _anchorPointInScreen(item, offsetX, offsetY)
    {
        if (!item || !item.mapToGlobal)
            return null

        const globalPoint = item.mapToGlobal(offsetX, offsetY)
        if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
            return globalPoint
        return null
    }

    function _menuItems()
    {
        return systemTray.trayMenuItems.filter(function(item) { return item.visible !== false })
    }

    function _availableHeightFromAnchor()
    {
        const screenGeometry = systemTray._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _rowHeight * Math.max(1, _menuItems().length)

        const minY = _margin
        const targetY = _trayMenuOriginY || (_margin + Maui.Style.toolBarHeightAlt)
        const startY = Math.max(minY, targetY)
        return Math.max(_rowHeight + (_panelInset * 2), screenGeometry.height - startY - _margin)
    }

    function _menuWidth()
    {
        const screenGeometry = systemTray._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return _preferredMenuWidth
        const available = Math.max(0, screenGeometry.width - (_margin * 2))
        return Math.max(_minimumMenuWidth, Math.min(_preferredMenuWidth, available))
    }

    function openTrayMenu(index, itemIsMenu, anchorItem)
    {
        trayMenuIndex = index
        trayMenuItemIsMenu = itemIsMenu
        trayMenuAnchor = anchorItem && anchorItem.popupAnchorMarker ? anchorItem.popupAnchorMarker : (anchorItem ? anchorItem : systemTray)
        trayMenuItems = systemTray.controller ? systemTray.controller.trayMenuItems(index) : []
        if (_trayMenuWindow.visible)
            _trayMenuWindow.close()

        const anchor = trayMenuAnchor ? trayMenuAnchor : systemTray
        const point = _anchorPointInScreen(anchor, 0, 0)
        const anchorHeight = anchor && anchor.height ? anchor.height : _rowHeight
        if (point)
        {
            _trayMenuOriginX = point.x
            _trayMenuOriginY = point.y + anchorHeight + Maui.Style.space.small
        }
        else
        {
            _trayMenuOriginX = _margin
            _trayMenuOriginY = _margin + Maui.Style.toolBarHeightAlt
        }

        Qt.callLater(function() {
            if (_trayMenuWindow)
                _trayMenuWindow.open()
        })
    }

    function closeTrayMenu()
    {
        _trayMenuWindow.close()
    }

    Repeater
    {
        model: systemTray.controller ? systemTray.controller : null

        delegate: ToolButton
        {
            required property int index
            required property string itemId
            required property string title
            required property string iconName
            required property string iconSource
            required property string status
            required property string service
            required property string objectPath
            required property string menu
            required property bool itemIsMenu

            function displayTitle()
            {
                if (title.length > 0 && !title.startsWith(":"))
                    return title

                if (status.length > 0 && !status.startsWith(":"))
                    return status

                return ""
            }

            function debugDetailsText()
            {
                const parts = []
                const display = displayTitle()
                if (display.length > 0)
                    parts.push("title: " + display)
                if (itemId.length > 0)
                    parts.push("itemId: " + itemId)
                if (service.length > 0)
                    parts.push("service: " + service)
                if (objectPath.length > 0)
                    parts.push("objectPath: " + objectPath)
                if (status.length > 0)
                    parts.push("status: " + status)
                if (iconName.length > 0)
                    parts.push("iconName: " + iconName)
                if (iconSource.length > 0)
                    parts.push("iconSource: " + iconSource)
                if (menu.length > 0)
                    parts.push("menu: " + menu)
                parts.push("itemIsMenu: " + itemIsMenu)
                return parts.join("\n")
            }

            icon.name: iconSource.length > 0 ? "" : (iconName.length > 0 ? iconName : "application-x-executable")
            icon.source: iconSource.length > 0 ? iconSource : ""
            display: ToolButton.IconOnly
            flat: true
            focusPolicy: Qt.NoFocus
            padding: Maui.Style.space.tiny
            icon.width: Maui.Style.iconSizes.small
            icon.height: Maui.Style.iconSizes.small

            onClicked:
            {
                if (!systemTray.controller)
                    return

                if (systemTray.debugDetails)
                {
                    systemTray.controller.debugTrayItem(index)
                    return
                }

                if (itemIsMenu)
                    systemTray.openTrayMenu(index, itemIsMenu, popupAnchorMarker)
                else
                    systemTray.controller.activate(index)
            }

            HoverHandler
            {
                id: _trayHover
            }

            ToolTip.delay: 0
            ToolTip.visible: systemTray.debugDetails && _trayHover.hovered
            ToolTip.text: debugDetailsText()

            MouseArea
            {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onPressed: function(mouse)
                {
                    if (!systemTray.controller || mouse.button !== Qt.RightButton)
                        return

                    mouse.accepted = true
                    systemTray.openTrayMenu(index, itemIsMenu, popupAnchorMarker)
                }
            }

            Item
            {
                id: _popupAnchorMarker
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                width: 1
                height: 1
                visible: true
            }

            readonly property Item popupAnchorMarker: _popupAnchorMarker

        }
    }

    Window
    {
        id: _trayMenuWindow
        color: "transparent"
        flags: Qt.FramelessWindowHint | Qt.Popup
        transientParent: systemTray.rootWindow ? systemTray.rootWindow : systemTray.Window.window
        visible: false

        x:
        {
            const dep = _geometryRevision
            const screenGeometry = systemTray._screenGeometry()
            if (!screenGeometry || screenGeometry.width <= 0)
                return _margin

            const targetX = _trayMenuOriginX || _margin
            const minX = _margin
            const maxX = Math.max(minX, screenGeometry.width - width - _margin)
            return Math.max(minX, Math.min(maxX, targetX))
        }

        y:
        {
            const dep = _geometryRevision
            const screenGeometry = systemTray._screenGeometry()
            if (!screenGeometry || screenGeometry.height <= 0)
                return _margin

            const targetY = _trayMenuOriginY || (_margin + Maui.Style.toolBarHeightAlt)
            const minY = _margin
            const maxY = Math.max(minY, screenGeometry.height - height - _margin)
            return Math.max(minY, Math.min(maxY, targetY))
        }

        width: systemTray._menuWidth()
        height:
        {
            const listHeight = _trayMenuList ? _trayMenuList.contentHeight : 0
            const contentHeight = listHeight > 0
                    ? (listHeight + (_panelInset * 2))
                    : ((_menuItems().length * _rowHeight) + (_panelInset * 2))
            return Math.min(contentHeight, systemTray._availableHeightFromAnchor())
        }

        function open()
        {
            if (visible)
                return

            if (systemTray.rootWindow && systemTray.rootWindow.closeTransientPopups)
                systemTray.rootWindow.closeTransientPopups()

            visible = true
            _menuOpen = true
            _touchGeometryRevision()
            requestActivate()
        }

        function close()
        {
            if (!visible)
                return

            visible = false
            _menuOpen = false
            _lastMenuClosedAtMs = Date.now()
        }

        function forceClose()
        {
            visible = false
            _menuOpen = false
            _lastMenuClosedAtMs = Date.now()
        }

        onVisibleChanged:
        {
            if (!visible)
                _lastMenuClosedAtMs = Date.now()
            else
                _touchGeometryRevision()
        }

        Connections
        {
            target: systemTray.rootWindow
            function onWidthChanged() { systemTray._touchGeometryRevision() }
            function onHeightChanged() { systemTray._touchGeometryRevision() }
            function onVisibilityChanged() { systemTray._touchGeometryRevision() }
            function onWindowStateChanged() { systemTray._touchGeometryRevision() }
        }

        Rectangle
        {
            id: _trayPanel
            anchors.fill: parent
            radius: Maui.Style.radiusV
            color: systemTray.rootWindow ? systemTray.rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
            border.width: 1
            border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
            clip: true

            layer.enabled: visible && GraphicsInfo.api !== GraphicsInfo.Software
            layer.effect: MultiEffect
            {
                autoPaddingEnabled: true
                shadowEnabled: false
                shadowColor: "#80000000"
            }

            ListView
            {
                id: _trayMenuList
                anchors.fill: parent
                anchors.margins: _panelInset
                model: systemTray._menuItems()
                spacing: 0
                currentIndex: -1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                delegate: MenuItem
                {
                    required property var modelData
                    width: ListView.view ? ListView.view.width : implicitWidth
                    text: String(modelData.label || "")
                    enabled: modelData.enabled !== false
                    checkable: modelData.checkable === true
                    checked: modelData.checked === true
                    display: ToolButton.TextOnly
                    padding: 0
                    leftPadding: Maui.Style.space.medium
                    rightPadding: Maui.Style.space.medium
                    topPadding: Maui.Style.space.tiny
                    bottomPadding: Maui.Style.space.tiny
                    icon.name: String(modelData.iconName || "")

                    onTriggered:
                    {
                        if (systemTray.controller && systemTray.trayMenuIndex >= 0)
                            systemTray.controller.triggerTrayMenuItem(systemTray.trayMenuIndex, Number(modelData.id))
                        _trayMenuWindow.close()
                    }
                }
            }
        }
    }
}
