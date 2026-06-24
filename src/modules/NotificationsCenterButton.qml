import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

ToolButton
{
    id: notificationsCenterButton

    property var popup
    property bool useSystemThemeIcons: true
    property string iconName: "notifications"
    property var glyphForIcon
    property string countText: ""
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property Item popupAnchorMarker: _popupAnchorMarker
    readonly property bool popupVisible: notificationsCenterButton.popup && notificationsCenterButton.popup.visible
    readonly property color activeContentColor: (notificationsCenterButton.down || notificationsCenterButton.popupVisible) ? Maui.Theme.highlightedTextColor : Maui.Theme.textColor

    display: ToolButton.IconOnly
    checked: popupVisible
    padding: Maui.Style.space.small
    ToolTip.visible: false
    ToolTip.text: ""

    function togglePopup()
    {
        if (!notificationsCenterButton.popup)
            return

        if (notificationsCenterButton.popup.visible)
        {
            notificationsCenterButton.popup.close()
            return
        }

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        notificationsCenterButton.popup.open()
    }

    onClicked:
    {
        togglePopup()
    }

    Connections
    {
        target: notificationsCenterButton.popup

        function onClosed()
        {
            notificationsCenterButton._lastClosedAtMs = Date.now()
        }
    }

    contentItem: RowLayout
    {
        spacing: Maui.Style.space.tiny

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            width: 20
            height: 20

            Maui.Icon
            {
                id: _notificationIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                source: notificationsCenterButton.iconName
                color: notificationsCenterButton.activeContentColor
                visible: notificationsCenterButton.useSystemThemeIcons && valid
            }

            Label
            {
                anchors.centerIn: parent
                visible: !notificationsCenterButton.useSystemThemeIcons || !_notificationIcon.valid
                text: notificationsCenterButton.glyphForIcon ? notificationsCenterButton.glyphForIcon(notificationsCenterButton.iconName) : ""
                color: notificationsCenterButton.activeContentColor
                font: Qt.font({ family: "Symbols Nerd Font", weight: Font.Normal, pixelSize: Math.max(10, Math.round(parent.height * 0.65)) })
                textFormat: Text.PlainText
                renderType: Text.QtRendering
            }
        }

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            width: _badgeRow.implicitWidth
            height: 20

            RowLayout
            {
                id: _badgeRow
                anchors.centerIn: parent
                spacing: 0

                WorkspaceBadge
                {
                    Layout.alignment: Qt.AlignVCenter
                    visible: notificationsCenterButton.countText.length > 0
                    badgeText: notificationsCenterButton.countText
                    bridge: null
                }
            }
        }
    }

    Item
    {
        id: _popupAnchorMarker
        anchors.right: parent.right
        anchors.top: parent.bottom
        width: 1
        height: 1
        visible: true

        Rectangle
        {
            anchors.fill: parent
            visible: false
            color: "#ff3b30"
        }
    }
}
