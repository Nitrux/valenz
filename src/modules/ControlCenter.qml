import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import org.mauikit.controls as Maui

Dialog
{
    id: controlCenter

    property Item anchorButton
    property var rootWindow
    property QtObject bridge
    property Item overlayItem: controlCenter.parent

    Maui.Theme.colorSet: Maui.Theme.View

    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInset: Maui.Style.contentMargins
    readonly property color _panelColor: Qt.alpha(Maui.Theme.backgroundColor, 0.76)
    readonly property string _nerdFontFamily: "Symbols Nerd Font Mono"
    readonly property int _minPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 20
    property int _geometryRevision: 0

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
    height: _panel.implicitHeight
    onAboutToShow:
    {
        _touchGeometryRevision()
    }
    onOpened:
    {
        Qt.callLater(_touchGeometryRevision)
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
        const maxY = Math.max(minY, overlayItem.height - height - _margin)
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin

        if (anchorButton)
        {
            const p = _anchorPointInOverlay(0, anchorButton.height)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return Math.max(minY, Math.min(maxY, targetY))
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
        implicitWidth: Math.max(controlCenter._minPanelWidth, _panelContent.implicitWidth + (controlCenter._panelInset * 2))
        implicitHeight: _panelContent.implicitHeight + (controlCenter._panelInset * 2)
        radius: Maui.Style.radiusV
        color: controlCenter._panelColor
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: "#80000000"
        }

        ColumnLayout
        {
            id: _panelContent
            anchors.fill: parent
            anchors.margins: controlCenter._panelInset
            spacing: Maui.Style.space.medium

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
                    text: "Alex R."
                    label2.text: "October 26, 2023 | 10:45 AM"
                    iconSource: "user-identity"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small

                        Item { Layout.fillWidth: true }

                        Maui.Icon
                        {
                            source: "battery"
                            color: Maui.Theme.textColor
                        }

                        Label
                        {
                            text: controlCenter.bridge ? controlCenter.bridge.controlCenterBatteryPercentage : "85%"
                            color: Maui.Theme.textColor
                        }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Bluetooth"
                    label2.text: controlCenter.bridge && controlCenter.bridge.prototypeBluetoothState === "off" ? "Off" : "On"
                    iconSource: "bluetooth-active"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small

                        ToolButton
                        {
                            text: "On"
                            checkable: true
                            checked: controlCenter.bridge && controlCenter.bridge.prototypeBluetoothState === "on"
                            onClicked:
                            {
                                if (controlCenter.bridge)
                                    controlCenter.bridge.prototypeBluetoothState = "on"
                            }
                        }

                        ToolButton
                        {
                            text: "Off"
                            checkable: true
                            checked: controlCenter.bridge && controlCenter.bridge.prototypeBluetoothState === "off"
                            onClicked:
                            {
                                if (controlCenter.bridge)
                                    controlCenter.bridge.prototypeBluetoothState = "off"
                            }
                        }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Cellular"
                    label2.text: "Off"
                    iconSource: "network-cellular-offline-symbolic"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small

                        ToolButton { text: "On"; checkable: true }
                        ToolButton { text: "Off"; checkable: true; checked: true }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "WiFi"
                    label2.text: "Home_Network"
                    iconSource: "network-wireless"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small
                        ToolButton { text: "On"; checkable: true; checked: true }
                        ToolButton { text: "Off"; checkable: true }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Do Not Disturb"
                    label2.text: "Off"
                    iconSource: "notifications-disabled"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small
                        ToolButton { text: "On"; checkable: true }
                        ToolButton { text: "Off"; checkable: true; checked: true }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Rotation Lock"
                    label2.text: "On"
                    iconSource: "rotation-locked"

                    Switch
                    {
                        checked: true
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Volume"
                    iconSource: "audio-volume-high"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small
                        Maui.Icon { source: "audio-volume-muted" }
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 62
                        }
                        Maui.Icon { source: "audio-volume-high" }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "Brightness"
                    iconSource: "display-brightness"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small
                        Maui.Icon { source: "brightness-low" }
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 70
                        }
                        Maui.Icon { source: "brightness-high" }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    flat: false
                    text: "Ambient Waves"
                    label2.text: "Synth Horizon"
                    iconSource: "media-playback-start"

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small

                        Item { Layout.fillWidth: true }

                        ToolButton
                        {
                            icon.name: "media-playback-start"
                            display: ToolButton.IconOnly
                        }
                        ToolButton
                        {
                            icon.name: "media-playback-pause"
                            display: ToolButton.IconOnly
                        }
                        ToolButton
                        {
                            icon.name: "media-skip-forward"
                            display: ToolButton.IconOnly
                        }
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: Maui.Style.space.small

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        text: "Night Light"
                        label2.text: "On"
                        iconSource: "weather-clear-night"

                        Switch
                        {
                            checked: true
                        }
                    }

                    Maui.SectionItem
                    {
                        Layout.fillWidth: true
                        flat: false
                        text: "Live Captions"
                        label2.text: "On"
                        iconSource: "comments"

                        Switch
                        {
                            checked: true
                        }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    flat: false
                    text: "CPU"
                    label2.text: "25%"
                    iconSource: "cpu"

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.tiny
                        Label { text: "RAM 60%"; color: Maui.Theme.textColor }
                        Label { text: "Disk 512GB / 1TB"; color: Maui.Theme.disabledTextColor }
                    }
                }

                Maui.SectionItem
                {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    flat: false
                    text: "Shortcuts"
                    label2.text: ""

                    Maui.ToolActions
                    {
                        Layout.fillWidth: true
                        display: ToolButton.IconOnly
                        expanded: true

                        Action
                        {
                            text: "Screen Record"
                            icon.name: "media-record"
                            onTriggered:
                            {
                                if (controlCenter.bridge)
                                    controlCenter.bridge.trace("controlCenter", "shortcut_screen_record")
                            }
                        }

                        Action
                        {
                            text: "Accessibility"
                            icon.name: "accessibility"
                            onTriggered:
                            {
                                if (controlCenter.bridge)
                                    controlCenter.bridge.trace("controlCenter", "shortcut_accessibility")
                            }
                        }

                        Action
                        {
                            text: "Settings"
                            icon.name: "settings-configure"
                            onTriggered:
                            {
                                if (controlCenter.bridge)
                                    controlCenter.bridge.trace("controlCenter", "shortcut_settings")
                            }
                        }
                    }
                }
            }
        }
    }
}
