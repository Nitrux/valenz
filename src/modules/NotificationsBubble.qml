import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{

    id: notificationsBubble

    property Item anchorButton
    property Item overlayItem
    property QtObject rootWindow
    property QtObject controller
    property var notificationsPopup
    property bool useSystemThemeIcons: true

    property int notificationId: -1
    property string sourceName: ""
    property string messageText: ""
    property string timestampText: "Now"
    property string iconName: "notifications"
    property int urgencyLevel: 0
    property string actionText: ""
    property string actionKey: ""

    property int _geometryRevision: 0

    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInsetX: 8
    readonly property int _panelInsetY: 8
    readonly property color _panelColor: Maui.Theme.backgroundColor
    readonly property int _preferredPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    readonly property real _targetY:
    {
        const screenGeometry = notificationsBubble._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _margin

        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorButton)
        {
            const p = _anchorPointInOverlay(0, anchorButton.height)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return targetY
    }
    readonly property real _availableWidth:
    {
        const screenGeometry = notificationsBubble._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return _preferredPanelWidth
        return Math.max(0, screenGeometry.width - (_margin * 2))
    }
    readonly property real _availableHeightFromAnchor:
    {
        const screenGeometry = notificationsBubble._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _panel.implicitHeight

        const minY = _margin
        const startY = Math.max(minY, _targetY)
        return Math.max(0, screenGeometry.height - startY - _margin)
    }

    readonly property int _timeoutMs:
    {
        if (urgencyLevel >= 2)
            return 7000
        if (urgencyLevel === 1)
            return 5500
        return 4200
    }

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted:
    {
        if (layerShellHelper)
            layerShellHelper.configurePopupWindow(notificationsBubble, "org.maui.valenz.bubble", false)
    }
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool


    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _anchorPointInOverlay(offsetX, offsetY)
    {
        const overlay = notificationsBubble.overlayItem
        if (!overlay || !anchorButton)
            return null

        if (anchorButton.mapToGlobal && overlay.mapFromGlobal)
        {
            const globalPoint = anchorButton.mapToGlobal(offsetX, offsetY)
            if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
            {
                const localPoint = overlay.mapFromGlobal(globalPoint.x, globalPoint.y)
                if (localPoint && isFinite(localPoint.x) && isFinite(localPoint.y))
                    return localPoint
            }
        }

        const mappedPoint = anchorButton.mapToItem(overlay, offsetX, offsetY)
        if (mappedPoint && isFinite(mappedPoint.x) && isFinite(mappedPoint.y))
            return mappedPoint

        return null
    }

    function _screenGeometry()
    {
        const screen = notificationsBubble.rootWindow ? notificationsBubble.rootWindow.screen : null
        if (screen && screen.availableGeometry && screen.availableGeometry.width > 0 && screen.availableGeometry.height > 0)
            return screen.availableGeometry
        if (screen && screen.geometry && screen.geometry.width > 0 && screen.geometry.height > 0)
            return screen.geometry
        return Qt.rect(0, 0, 0, 0)
    }

    function _notificationGlyph(iconNameValue)
    {
        switch (String(iconNameValue))
        {
            case "bluetooth-active": return "\uF294"
            case "battery": return "\uF240"
            case "settings-configure": return "\uF013"
            default: return "\uF0F3"
        }
    }

    function _resolvedIconSource(iconNameValue)
    {
        const raw = String(iconNameValue || "").trim()
        if (raw.length === 0)
            return ""

        if (raw.startsWith("qrc:/app/valenz/modules/"))
            return raw.substring(raw.lastIndexOf("/") + 1)

        if (raw.startsWith("/"))
            return "file://" + raw

        return raw
    }

    function _isImageIconSource(iconNameValue)
    {
        const source = _resolvedIconSource(iconNameValue)
        return source.startsWith("file://")
                || source.startsWith("qrc:/")
                || source.startsWith(":/")
                || source.startsWith("data:")
                || source.startsWith("http://")
                || source.startsWith("https://")
                || source.startsWith("image://")
    }

    function _urgencyAccentColor(level)
    {
        switch (level)
        {
            case 2: return Maui.Theme.negativeBackgroundColor
            case 1: return Maui.Theme.highlightColor
            case 0: return Qt.alpha(Maui.Theme.textColor, 0.28)
            default: return "transparent"
        }
    }

    function showNotification(idValue, sourceNameValue, messageTextValue, timestampTextValue, iconNameValue, urgencyLevelValue, actionTextValue, actionKeyValue)
    {
        console.log("NotificationsBubble.showNotification", idValue, sourceNameValue, "dnd=", controller ? controller.dndEnabled : false, "popupVisible=", notificationsPopup ? notificationsPopup.visible : false)
        if (notificationsPopup && notificationsPopup.visible)
            return

        notificationId = Number(idValue)
        sourceName = String(sourceNameValue || "").trim()
        messageText = String(messageTextValue || "").trim()
        timestampText = String(timestampTextValue || "").trim()
        iconName = String(iconNameValue || "").trim()
        urgencyLevel = Number(urgencyLevelValue)
        actionText = String(actionTextValue || "").trim()
        actionKey = String(actionKeyValue || "").trim()

        if (!isFinite(notificationId))
            notificationId = -1
        if (sourceName.length === 0)
            sourceName = "Notification"
        if (messageText.length === 0)
            messageText = "(No details)"
        if (timestampText.length === 0)
            timestampText = "Now"
        if (iconName.length === 0)
            iconName = "notifications"
        if (!isFinite(urgencyLevel))
            urgencyLevel = 0

        if (_bubbleContentColumn && _bubbleContentColumn.forceLayout)
            _bubbleContentColumn.forceLayout()

        _touchGeometryRevision()
        open()

        Qt.callLater(function()
        {
            if (_bubbleContentColumn && _bubbleContentColumn.forceLayout)
                _bubbleContentColumn.forceLayout()

            notificationsBubble._touchGeometryRevision()
        })

        _autoCloseTimer.restart()
    }

    function dismissBubble()
    {
        console.log("NotificationsBubble.dismissBubble", "visible=", visible, "popupVisible=", notificationsPopup ? notificationsPopup.visible : false, "controllerDnd=", controller ? controller.dndEnabled : false)
        _autoCloseTimer.stop()
        if (visible)
            close()
    }

    onActionTextChanged:
    {
        if (_bubbleContentColumn && _bubbleContentColumn.forceLayout)
            _bubbleContentColumn.forceLayout()

        if (visible)
            _touchGeometryRevision()
    }

    onMessageTextChanged:
    {
        if (_bubbleContentColumn && _bubbleContentColumn.forceLayout)
            _bubbleContentColumn.forceLayout()

        if (visible)
            _touchGeometryRevision()
    }

    signal aboutToShow()
    signal opened()
    signal closed()

    function open()
    {
        if (visible)
            return

        aboutToShow()
        visible = true
        requestActivate()
    }

    function close()
    {
        if (!visible)
            return

        visible = false
    }

    width:
    {
        if (_availableWidth <= 0)
            return _preferredPanelWidth

        return Math.min(_preferredPanelWidth, _availableWidth)
    }
    height: Math.min(_panel.implicitHeight, _availableHeightFromAnchor)


    x:
    {
        const dep = _geometryRevision
        const screenGeometry = notificationsBubble._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        const minX = _margin
        const maxX = Math.max(minX, screenGeometry.width - width - _margin)
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
        const overlay = notificationsBubble.overlayItem
        if (!overlay)
            return _margin

        const minY = _margin
        return Math.max(minY, _targetY)
    }

    Timer
    {
        id: _autoCloseTimer
        interval: notificationsBubble._timeoutMs
        repeat: false
        onTriggered: notificationsBubble.dismissBubble()
    }

    Connections
    {
        target: notificationsBubble.anchorButton

        function onXChanged() { notificationsBubble._touchGeometryRevision() }
        function onYChanged() { notificationsBubble._touchGeometryRevision() }
        function onWidthChanged() { notificationsBubble._touchGeometryRevision() }
        function onHeightChanged() { notificationsBubble._touchGeometryRevision() }
        function onVisibleChanged() { notificationsBubble._touchGeometryRevision() }
    }

    Connections
    {
        target: notificationsBubble.overlayItem

        function onWidthChanged() { notificationsBubble._touchGeometryRevision() }
        function onHeightChanged() { notificationsBubble._touchGeometryRevision() }
        function onXChanged() { notificationsBubble._touchGeometryRevision() }
        function onYChanged() { notificationsBubble._touchGeometryRevision() }
    }

    Connections
    {
        target: notificationsBubble.rootWindow

        function onWidthChanged() { notificationsBubble._touchGeometryRevision() }
        function onHeightChanged() { notificationsBubble._touchGeometryRevision() }
        function onVisibilityChanged() { notificationsBubble._touchGeometryRevision() }
        function onWindowStateChanged() { notificationsBubble._touchGeometryRevision() }
    }

    Connections
    {
        target: notificationsBubble.notificationsPopup

        function onVisibleChanged()
        {
            if (notificationsBubble.notificationsPopup && notificationsBubble.notificationsPopup.visible)
                notificationsBubble.dismissBubble()
        }
    }

    Connections
    {
        target: notificationsBubble.controller

        function onDndEnabledChanged()
        {
            console.log("NotificationsBubble.onDndEnabledChanged", notificationsBubble.controller ? notificationsBubble.controller.dndEnabled : false)
            if (notificationsBubble.controller && notificationsBubble.controller.dndEnabled)
                notificationsBubble.dismissBubble()
        }
    }

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        implicitWidth: notificationsBubble.width
        implicitHeight: _bubbleCard.implicitHeight + (notificationsBubble._panelInsetY * 2)
        radius: Maui.Style.radiusV
        color: notificationsBubble._panelColor
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: "#80000000"
        }

        Rectangle
        {
            id: _bubbleCard
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: notificationsBubble._panelInsetX
            anchors.rightMargin: notificationsBubble._panelInsetX
            anchors.topMargin: notificationsBubble._panelInsetY
            property int padding: Math.max(Maui.Style.space.medium, Maui.Style.space.small + 2)
            implicitHeight: (_bubbleContentColumn ? _bubbleContentColumn.implicitHeight : 0) + (padding * 2)

            color: Maui.Theme.alternateBackgroundColor
            radius: Maui.Style.radiusV
            border.width: 1
            border.color: notificationsBubble._urgencyAccentColor(notificationsBubble.urgencyLevel)

            ColumnLayout
            {
                id: _bubbleContentColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: _bubbleCard.padding
                anchors.rightMargin: _bubbleCard.padding
                anchors.topMargin: _bubbleCard.padding
                spacing: Maui.Style.space.small

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: Maui.Style.space.small

                    Item
                    {
                        Layout.alignment: Qt.AlignVCenter
                        width: 28
                        height: 28

                        readonly property bool isImageSource: notificationsBubble._isImageIconSource(notificationsBubble.iconName)
                        readonly property string resolvedIconSource: notificationsBubble._resolvedIconSource(notificationsBubble.iconName)

                        Image
                        {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            source: parent.isImageSource ? parent.resolvedIconSource : ""
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: parent.isImageSource
                        }

                        Maui.Icon
                        {
                            id: _notificationIcon
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            source: parent.resolvedIconSource
                            color: Maui.Theme.textColor
                            visible: !parent.isImageSource && notificationsBubble.useSystemThemeIcons && valid
                        }

                        Label
                        {
                            anchors.centerIn: parent
                            visible: !parent.isImageSource && (!notificationsBubble.useSystemThemeIcons || !_notificationIcon.valid)
                            text: notificationsBubble._notificationGlyph(notificationsBubble.iconName)
                            color: Maui.Theme.textColor
                            font.family: "Symbols Nerd Font"
                            font.weight: Font.Normal
                            font.pixelSize: Math.max(12, Math.round(parent.height * 0.75))
                            textFormat: Text.PlainText
                            renderType: Text.QtRendering
                        }
                    }

                    Label
                    {
                        Layout.fillWidth: true
                        text: notificationsBubble.sourceName
                        color: Maui.Theme.textColor
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label
                    {
                        Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        text: notificationsBubble.timestampText
                        color: Qt.alpha(Maui.Theme.textColor, 0.7)
                    }

                    ToolButton
                    {
                        id: _dismissButton
                        Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        display: ToolButton.IconOnly
                        padding: 0
                        implicitWidth: 16
                        implicitHeight: 16
                        icon.source: "qrc:/assets/close.svg"
                        icon.width: 16
                        icon.height: 16
                        icon.color: hovered || down ? Maui.Theme.negativeTextColor : Maui.Theme.textColor
                        background: Rectangle
                        {
                            radius: Maui.Style.radiusV
                            color: _dismissButton.hovered || _dismissButton.down ? Maui.Theme.negativeBackgroundColor : "transparent"
                        }
                        onClicked: notificationsBubble.dismissBubble()
                    }
                }

                Label
                {
                    Layout.fillWidth: true
                    text: notificationsBubble.messageText
                    color: Qt.alpha(Maui.Theme.textColor, 0.87)
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }

                RowLayout
                {
                    Layout.fillWidth: true

                    Item
                    {
                        Layout.fillWidth: true
                    }

                    Button
                    {
                        visible: notificationsBubble.actionText.length > 0
                        text: notificationsBubble.actionText
                        onClicked:
                        {
                            if (notificationsBubble.controller && notificationsBubble.notificationId >= 0)
                                notificationsBubble.controller.invokeActionById(notificationsBubble.notificationId)

                            notificationsBubble.dismissBubble()
                        }
                    }
                }
            }
        }
    }
}
