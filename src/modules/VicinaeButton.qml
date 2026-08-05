import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

ToolButton
{
    id: vicinaeButton

    property bool useSystemThemeIcons: true
    property string iconName: "system-search"
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

    contentItem: Item
    {
        implicitWidth: 20
        implicitHeight: 20

        Maui.Icon
        {
            id: _vicinaeIcon
            anchors.centerIn: parent
            width: 16
            height: 16
            source: vicinaeButton.iconName
            color: vicinaeButton.activeContentColor
            visible: vicinaeButton.useSystemThemeIcons && valid
        }

        Label
        {
            anchors.centerIn: parent
            visible: !vicinaeButton.useSystemThemeIcons || !_vicinaeIcon.valid
            text: vicinaeButton.glyphForIcon ? vicinaeButton.glyphForIcon(vicinaeButton.iconName) : ""
            color: vicinaeButton.activeContentColor
            font.family: "Symbols Nerd Font"
            font.weight: Font.Normal
            font.pointSize: Math.max(7, Math.round(parent.height * 0.65 * 0.75))
            textFormat: Text.PlainText
            renderType: Text.QtRendering
        }
    }
}
