import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

ToolButton
{
    id: vicinaeButton

    property bool useSystemThemeIcons: true
    property var glyphForIcon
    readonly property color activeContentColor: vicinaeButton.down ? Maui.Theme.highlightedTextColor : Maui.Theme.textColor

    display: ToolButton.IconOnly
    padding: Maui.Style.space.small
    ToolTip.visible: false
    ToolTip.text: ""

    onClicked:
    {
        if (valenzBridge)
            valenzBridge.toggleVicinae()
    }


    contentItem: RowLayout
    {
        spacing: 16

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            width: 16
            height: 16

            Maui.Icon
            {
                id: _launcherIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "system-search"
                color: vicinaeButton.activeContentColor
                visible: vicinaeButton.useSystemThemeIcons && valid
            }

            Label
            {
                anchors.centerIn: parent
                visible: !vicinaeButton.useSystemThemeIcons || !_launcherIcon.valid
                text: vicinaeButton.glyphForIcon ? vicinaeButton.glyphForIcon("system-search") : ""
                color: vicinaeButton.activeContentColor
                font.family: "Symbols Nerd Font"
                font.weight: Font.Normal
                font.pointSize: Math.max(7, Math.round(parent.height * 0.65 * 0.75))
                textFormat: Text.PlainText
                renderType: Text.QtRendering
            }

            TapHandler
            {
                onTapped:
                {
                    if (valenzBridge)
                        valenzBridge.toggleVicinae()
                }
            }
        }

        Item
        {
            Layout.alignment: Qt.AlignVCenter
            width: 16
            height: 16

            Maui.Icon
            {
                id: _clipboardIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "edit-paste"
                color: vicinaeButton.activeContentColor
                visible: vicinaeButton.useSystemThemeIcons && valid
            }

            Label
            {
                anchors.centerIn: parent
                visible: !vicinaeButton.useSystemThemeIcons || !_clipboardIcon.valid
                text: vicinaeButton.glyphForIcon ? vicinaeButton.glyphForIcon("edit-paste") : ""
                color: vicinaeButton.activeContentColor
                font.family: "Symbols Nerd Font"
                font.weight: Font.Normal
                font.pointSize: Math.max(7, Math.round(parent.height * 0.65 * 0.75))
                textFormat: Text.PlainText
                renderType: Text.QtRendering
            }

            TapHandler
            {
                onTapped:
                {
                    if (valenzBridge)
                        valenzBridge.openVicinaeClipboard()
                }
            }
        }
    }
}
