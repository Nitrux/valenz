import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

ToolButton
{
    id: screenCaptureButton

    property var popup
    property bool useSystemThemeIcons: true
    property var glyphForIcon
    property string iconName: "valenz-screen-capture"
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property Item popupAnchorMarker: _popupAnchorMarker
    readonly property bool popupVisible: screenCaptureButton.popup && screenCaptureButton.popup.visible
    readonly property bool recordingActive: screenCaptureButton.popup && screenCaptureButton.popup.recordingActive
    readonly property string recordingTimeText: screenCaptureButton.popup ? screenCaptureButton.popup.recordingTimeText : "0:00"
    readonly property color activeContentColor: (screenCaptureButton.down || screenCaptureButton.popupVisible) ? Maui.Theme.highlightedTextColor : Maui.Theme.textColor

    display: ToolButton.IconOnly
    padding: Maui.Style.space.small
    checked: popupVisible
    ToolTip.visible: false
    ToolTip.text: ""

    function togglePopup()
    {
        if (!screenCaptureButton.popup)
            return

        if (screenCaptureButton.popup.visible)
        {
            screenCaptureButton.popup.close()
            return
        }

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        screenCaptureButton.popup.open()
    }

    onClicked: togglePopup()

    Connections
    {
        target: screenCaptureButton.popup

        function onClosed()
        {
            screenCaptureButton._lastClosedAtMs = Date.now()
        }
    }

    contentItem: RowLayout
    {
        spacing: 3

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            width: 16
            height: 16

            Maui.Icon
            {
                id: _captureIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                source: screenCaptureButton.iconName
                color: screenCaptureButton.activeContentColor
                visible: screenCaptureButton.useSystemThemeIcons && valid
            }

            Label
            {
                anchors.centerIn: parent
                visible: !screenCaptureButton.useSystemThemeIcons || !_captureIcon.valid
                text: screenCaptureButton.glyphForIcon ? screenCaptureButton.glyphForIcon(screenCaptureButton.iconName) : ""
                color: screenCaptureButton.activeContentColor
                font.family: "Symbols Nerd Font"
                font.weight: Font.Normal
                font.pointSize: Math.max(7, Math.round(parent.height * 0.65 * 0.75))
                textFormat: Text.PlainText
                renderType: Text.QtRendering
            }
        }

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            visible: screenCaptureButton.recordingActive
            implicitWidth: visible ? _recordingBadge.implicitWidth : 0
            Layout.minimumWidth: implicitWidth
            Layout.preferredWidth: implicitWidth
            height: 20

            WorkspaceBadge
            {
                id: _recordingBadge
                anchors.centerIn: parent
                badgeText: "REC " + screenCaptureButton.recordingTimeText
                bridge: null
                Maui.Controls.status: Maui.Controls.Negative

                background: Rectangle
                {
                    color: Maui.Theme.negativeBackgroundColor
                    radius: Maui.Style.radiusV
                }

                contentItem: Maui.IconLabel
                {
                    text: _recordingBadge.text
                    font: _recordingBadge.font
                    color: Maui.Theme.negativeTextColor
                    alignment: Qt.AlignHCenter
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
