import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window

import org.mauikit.controls as Maui

Window
{

    id: notificationsCenter

    property Item anchorButton
    property var rootWindow
    property Item overlayItem
    property QtObject controller
    property bool useSystemThemeIcons: true

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted: { }
    color: "transparent"

    visible: false
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Shortcut
    {
        sequences: [ StandardKey.Cancel ]
        onActivated: notificationsCenter.close()
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
    readonly property color _panelColor: notificationsCenter.rootWindow ? notificationsCenter.rootWindow.popupSurfaceColor : Maui.Theme.backgroundColor
    readonly property int _cardPadding: Math.max(Maui.Style.space.medium, Maui.Style.space.small + 2)
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    readonly property int notificationCount: controller ? controller.count : _notificationsModel.count
    property int _geometryRevision: 0
    property bool _clearingAll: false
    property bool _fadeOutPending: false
    property bool _panelOpen: false

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100
    property int clearAllTrigger: 0
    property int _clearRemainingAnimations: 0
    readonly property int _clearAllStaggerMs: 45

    readonly property real _targetY:
    {
        const overlayItem = notificationsCenter.overlayItem
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
        const screenGeometry = notificationsCenter._screenGeometry()
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
        const screen = notificationsCenter.rootWindow ? notificationsCenter.rootWindow.screen : null
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

    function _notificationGlyph(iconName)
    {
        switch (String(iconName))
        {
            case "bluetooth-active": return "\uF294"
            case "battery": return "\uF240"
            case "settings-configure": return "\uF013"
            default: return "\uF0F3"
        }
    }

    function _resolvedIconSource(iconName)
    {
        const raw = String(iconName || "").trim()
        if (raw.length === 0)
            return ""

        if (raw.startsWith("qrc:/app/valenz/modules/"))
            return raw.substring(raw.lastIndexOf("/") + 1)

        if (raw.startsWith("/"))
            return "file://" + raw

        return raw
    }

    function _isImageIconSource(iconName)
    {
        const source = _resolvedIconSource(iconName)
        return source.startsWith("file://")
                || source.startsWith("qrc:/")
                || source.startsWith(":/")
                || source.startsWith("data:")
                || source.startsWith("http://")
                || source.startsWith("https://")
                || source.startsWith("image://")
    }

    function _urgencyAccentColor(urgencyLevel)
    {
        switch (urgencyLevel)
        {
            case 2: return Maui.Theme.negativeBackgroundColor
            case 1: return Maui.Theme.highlightColor
            case 0: return Qt.alpha(Maui.Theme.textColor, 0.28)
            default: return "transparent"
        }
    }

    function dismissNotification(index)
    {
        if (controller)
        {
            controller.dismiss(index)
            return
        }

        if (index >= 0 && index < _notificationsModel.count)
            _notificationsModel.remove(index, 1)
    }

    function _onClearAllCardDismissed()
    {
        if (!_clearingAll)
            return

        _clearRemainingAnimations = Math.max(0, _clearRemainingAnimations - 1)

        if (_clearRemainingAnimations === 0)
        {
            if (controller)
                controller.clearAllNotifications()
            else
                _notificationsModel.clear()

            _clearingAll = false
        }
    }

    function clearNotifications()
    {
        if (_clearingAll || notificationCount === 0)
            return

        _clearRemainingAnimations = notificationCount
        _clearingAll = true
        clearAllTrigger += 1
    }

    ListModel
    {
        id: _notificationsModel
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
        interval: notificationsCenter._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (notificationsCenter._fadeOutPending)
            {
                notificationsCenter._fadeOutPending = false
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
            if (controller)
                controller.refreshTimestamps()
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            closed()
        }
    }


    x:
    {
        const dep = _geometryRevision
        const screenGeometry = notificationsCenter._screenGeometry()
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
        const overlayItem = notificationsCenter.overlayItem
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
        target: notificationsCenter.anchorButton

        function onXChanged() { notificationsCenter._touchGeometryRevision() }
        function onYChanged() { notificationsCenter._touchGeometryRevision() }
        function onWidthChanged() { notificationsCenter._touchGeometryRevision() }
        function onHeightChanged() { notificationsCenter._touchGeometryRevision() }
        function onVisibleChanged() { notificationsCenter._touchGeometryRevision() }
    }

    Connections
    {
        target: notificationsCenter.overlayItem

        function onWidthChanged() { notificationsCenter._touchGeometryRevision() }
        function onHeightChanged() { notificationsCenter._touchGeometryRevision() }
        function onXChanged() { notificationsCenter._touchGeometryRevision() }
        function onYChanged() { notificationsCenter._touchGeometryRevision() }
    }

    Connections
    {
        target: notificationsCenter.rootWindow

        function onWidthChanged() { notificationsCenter._touchGeometryRevision() }
        function onHeightChanged() { notificationsCenter._touchGeometryRevision() }
        function onVisibilityChanged() { notificationsCenter._touchGeometryRevision() }
        function onWindowStateChanged() { notificationsCenter._touchGeometryRevision() }
    }
    Rectangle
    {
        id: _panel
        anchors.fill: parent
        opacity: 0.0
        scale: 0.97
        transformOrigin: Item.Center
        implicitWidth: Math.max(notificationsCenter._minPanelWidth, _panelContent.implicitWidth + (notificationsCenter._panelInsetX * 2))
        implicitHeight: _panelContent.implicitHeight + (notificationsCenter._panelInsetY * 2)
        radius: Maui.Style.radiusV + 3
        color: notificationsCenter._panelColor
        border.width: 1
        border.color: Qt.alpha(Maui.Theme.textColor, 0.10)
        states: [
            State
            {
                name: "open"
                when: notificationsCenter._panelOpen
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
                when: !notificationsCenter._panelOpen
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
                duration: notificationsCenter._panelOpen ? notificationsCenter._fadeInDurationMs : notificationsCenter._fadeOutDurationMs
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

        ColumnLayout
        {
            id: _panelContent
            anchors.fill: parent
            anchors.leftMargin: notificationsCenter._panelInsetX
            anchors.rightMargin: notificationsCenter._panelInsetX
            anchors.topMargin: notificationsCenter._panelInsetY
            anchors.bottomMargin: notificationsCenter._panelInsetY
            spacing: Maui.Style.space.small

            Maui.SectionItem
            {
                Layout.fillWidth: true
                flat: false
                clip: true
                padding: notificationsCenter._cardPadding
                text: ""
                label2.text: ""

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: Maui.Style.space.small

                    ColumnLayout
                    {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Label
                        {
                            text: i18n("Notifications")
                            color: Maui.Theme.textColor
                            font.weight: Font.DemiBold
                        }

                        Label
                        {
                            text: i18n("Recent activity")
                            color: Qt.alpha(Maui.Theme.textColor, 0.85)
                        }
                    }

                    Item
                    {
                        Layout.fillWidth: true
                    }

                    Button
                    {
                        id: _clearAllButton
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        enabled: notificationsCenter.notificationCount > 0 && !notificationsCenter._clearingAll
                        onClicked: notificationsCenter.clearNotifications()

                        contentItem: RowLayout
                        {
                            spacing: Maui.Style.space.tiny

                            Item
                            {
                                Layout.alignment: Qt.AlignVCenter
                                width: 18
                                height: 18

                                Maui.Icon
                                {
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16
                                    source: "edit-clear-all"
                                    color: _clearAllButton.enabled ? Maui.Theme.textColor : Maui.Theme.disabledTextColor
                                    visible: notificationsCenter.useSystemThemeIcons
                                }

                                Label
                                {
                                    anchors.centerIn: parent
                                    visible: !notificationsCenter.useSystemThemeIcons
                                    text: "\uee23"
                                    color: _clearAllButton.enabled ? Maui.Theme.textColor : Maui.Theme.disabledTextColor
                                    font: Qt.font({ family: "Symbols Nerd Font", weight: Font.Normal, pixelSize: 12 })
                                    textFormat: Text.PlainText
                                    renderType: Text.QtRendering
                                }
                            }

                            Label
                            {
                                Layout.alignment: Qt.AlignVCenter
                                text: i18n("Clear All")
                                color: _clearAllButton.enabled ? Maui.Theme.textColor : Maui.Theme.disabledTextColor
                            }
                        }
                    }
                }
            }

            Flickable
            {
                id: _notificationsFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: _notificationsColumn.implicitHeight
                contentWidth: width
                contentHeight: _notificationsColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                Column
                {
                    id: _notificationsColumn
                    width: _notificationsFlick.width
                    spacing: Maui.Style.space.small

                    Maui.SectionItem
                    {
                        width: parent.width
                        visible: notificationsCenter.notificationCount === 0
                        flat: false
                clip: true
                        padding: notificationsCenter._cardPadding
                        text: i18n("No notifications")
                        label2.text: i18n("You are all caught up.")
                    }

                    Repeater
                    {
                        model: notificationsCenter.controller ? notificationsCenter.controller : _notificationsModel

                        delegate: Maui.SectionItem
                        {
                            id: _notificationCard
                            required property int index
                            required property string sourceName
                            required property string messageText
                            required property string timestampText
                            required property string iconName
                            required property int urgencyLevel
                            required property string actionText
                            required property string actionKey

                            property bool _dismissing: false
                            property int _dismissDelayMs: 0

                            function startDismiss(delayMs)
                            {
                                if (_dismissing)
                                    return

                                _dismissing = true
                                _dismissDelayMs = Math.max(0, delayMs || 0)
                                _dismissAnimation.start()
                            }

                            Connections
                            {
                                target: notificationsCenter

                                function onClearAllTriggerChanged()
                                {
                                    if (notificationsCenter._clearingAll)
                                        _notificationCard.startDismiss(index * notificationsCenter._clearAllStaggerMs)
                                }
                            }

                            width: _notificationsColumn.width
                            flat: false
                clip: true
                            padding: notificationsCenter._cardPadding
                            text: ""
                            label2.text: ""
                            background: Rectangle
                            {
                                color: Maui.Theme.alternateBackgroundColor
                                radius: Maui.Style.radiusV
                                border.width: urgencyLevel >= 0 ? 1 : 0
                                border.color: notificationsCenter._urgencyAccentColor(urgencyLevel)
                            }
                            SequentialAnimation
                            {
                                id: _dismissAnimation

                                PauseAnimation
                                {
                                    duration: _notificationCard._dismissDelayMs
                                }

                                ParallelAnimation
                                {
                                    NumberAnimation
                                    {
                                        target: _notificationCard
                                        property: "x"
                                        to: _notificationCard.width + Maui.Style.space.large
                                        duration: 200
                                        easing.type: Easing.InOutCubic
                                    }

                                    NumberAnimation
                                    {
                                        target: _notificationCard
                                        property: "opacity"
                                        to: 0
                                        duration: 160
                                        easing.type: Easing.InOutQuad
                                    }
                                }

                                onFinished:
                                {
                                    if (notificationsCenter._clearingAll)
                                        notificationsCenter._onClearAllCardDismissed()
                                    else
                                        notificationsCenter.dismissNotification(index)
                                }
                            }

                            ColumnLayout
                            {
                                Layout.fillWidth: true
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

                                        readonly property bool isImageSource: notificationsCenter._isImageIconSource(iconName)
                                        readonly property string resolvedIconSource: notificationsCenter._resolvedIconSource(iconName)

                                        Maui.IconItem
                                        {
                                            id: _notificationIcon
                                            anchors.centerIn: parent
                                            width: 22
                                            height: 22
                                            imageSource: parent.isImageSource ? parent.resolvedIconSource : ""
                                            iconSource: parent.isImageSource ? "" : (notificationsCenter.useSystemThemeIcons ? iconName : "")
                                            imageSizeHint: 22
                                            iconSizeHint: 22
                                            color: Maui.Theme.textColor
                                        }

                                        Label
                                        {
                                            anchors.centerIn: parent
                                            visible: !parent.isImageSource && (!notificationsCenter.useSystemThemeIcons || !_notificationIcon.icon.valid)
                                            text: notificationsCenter._notificationGlyph(iconName)
                                            color: Maui.Theme.textColor
                                            font: Qt.font({ family: "Symbols Nerd Font", weight: Font.Normal, pixelSize: Math.max(12, Math.round(parent.height * 0.75)) })
                                            textFormat: Text.PlainText
                                            renderType: Text.QtRendering
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label
                                        {
                                            Layout.fillWidth: true
                                            text: sourceName
                                            color: Maui.Theme.textColor
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Label
                                    {
                                        Layout.alignment: Qt.AlignTop | Qt.AlignRight
                                        text: timestampText
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
                                        enabled: !_notificationCard._dismissing && !notificationsCenter._clearingAll
                                        onClicked: _notificationCard.startDismiss(0)
                                    }
                                }

                                Label
                                {
                                    Layout.fillWidth: true
                                    text: messageText
                                    color: Qt.alpha(Maui.Theme.textColor, 0.87)
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
                                        visible: actionText.length > 0
                                        text: actionText
                                        onClicked:
                                        {
                                            if (notificationsCenter.controller)
                                            {
                                                notificationsCenter.controller.invokeAction(index)
                                                return
                                            }

                                            if (notificationsCenter.rootWindow && notificationsCenter.rootWindow.traceMenu)
                                                notificationsCenter.rootWindow.traceMenu("notification_action", actionKey)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
