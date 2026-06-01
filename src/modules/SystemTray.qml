import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

RowLayout
{
    id: systemTray

    property QtObject controller

    spacing: Maui.Style.space.tiny
    visible: !!controller && controller.count > 0

    Repeater
    {
        model: systemTray.controller ? systemTray.controller : null

        delegate: ToolButton
        {
            required property int index
            required property string title
            required property string iconName
            required property string iconSource
            required property string status

            icon.name: iconSource.length > 0 ? "" : (iconName.length > 0 ? iconName : "application-x-executable")
            icon.source: iconSource.length > 0 ? iconSource : ""
            display: ToolButton.IconOnly
            flat: true
            focusPolicy: Qt.NoFocus

            onClicked:
            {
                if (systemTray.controller)
                    systemTray.controller.activate(index)
            }

            TapHandler
            {
                acceptedButtons: Qt.RightButton
                onTapped:
                {
                    if (systemTray.controller)
                        systemTray.controller.contextMenu(index)
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: title.length > 0 ? title : status
        }
    }
}
