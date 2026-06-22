import QtQuick
import QtQuick.Controls

import org.mauikit.controls as Maui

Maui.ToolActions
{
    id: workspaceNavigation

    property QtObject bridge

    display: ToolButton.IconOnly
    checkable: false
    autoExclusive: false

    Action
    {
        text: i18n("Previous workspace")
        icon.name: "go-previous"
        enabled: !!workspaceNavigation.bridge
        onTriggered:
        {
            workspaceNavigation.bridge.goToPreviousWorkspace()
        }
    }

    Action
    {
        text: i18n("Next workspace")
        icon.name: "go-next"
        enabled: !!workspaceNavigation.bridge
        onTriggered:
        {
            workspaceNavigation.bridge.goToNextWorkspace()
        }
    }
}
