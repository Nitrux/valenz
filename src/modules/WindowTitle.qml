import QtQuick
import QtQuick.Controls

import org.mauikit.controls as Maui

Item
{
    id: windowTitle

    property QtObject bridge
    property QtObject workspacePopup
    property string fallbackTitle: ""
    property int referenceHeight: -1
    readonly property int tabHeight: Math.min(28, referenceHeight > 0 ? referenceHeight : 28)
    readonly property var windows: bridge ? bridge.windowList : []
    readonly property bool hasWindows: windows.length > 0
    readonly property int minimumTabWidth: 96

    function preferredTabWidth(windowData)
    {
        const title = cleanText(windowData && windowData.title)
        const textWidth = title.length * 7
        return Math.max(minimumTabWidth, Math.min(240, textWidth + (Maui.Style.iconSizes.small * 2) + (Maui.Style.space.big * 2) + Maui.Style.space.small))
    }

    readonly property real preferredWidth:
    {
        var width = 0
        for (var i = 0; i < windows.length; ++i)
            width += preferredTabWidth(windows[i])
        return width
    }

    visible: hasWindows
    implicitWidth: hasWindows
                   ? Math.max(minimumTabWidth * windows.length, preferredWidth)
                   : 0
    implicitHeight: hasWindows ? tabHeight : 0

    ListModel
    {
        id: tabModel

        function sync(windows)
        {
            for (let i = 0; i < windows.length; ++i)
            {
                const entry = windows[i]
                let row = i
                while (row < count && get(row).address !== entry.address)
                    ++row

                if (row === count)
                    insert(i, entry)
                else
                {
                    if (row !== i)
                        move(row, i, 1)
                    set(i, entry)
                }
            }

            if (count > windows.length)
                remove(windows.length, count - windows.length)
        }

        Component.onCompleted: sync(windowTitle.windows)
    }

    onWindowsChanged: if (tabModel) tabModel.sync(windows)

    function cleanText(value)
    {
        return String(value || "").trim()
    }

    function toggleWorkspacePopup(anchorButton, address)
    {
        if (!workspacePopup || !anchorButton)
            return

        const point = anchorButton.mapToItem(windowTitle, anchorButton.width / 2, anchorButton.height / 2)
        if (!point || !isFinite(point.x) || !isFinite(point.y))
            return

        _workspacePopupAnchorMarker.x = point.x - (_workspacePopupAnchorMarker.width / 2)
        _workspacePopupAnchorMarker.y = point.y - (_workspacePopupAnchorMarker.height / 2)
        workspacePopup.toggleFromAnchor(_workspacePopupAnchorMarker, address)
    }

    Item
    {
        id: _workspacePopupAnchorMarker
        width: Maui.Style.space.tiny
        height: Maui.Style.space.tiny
        visible: true

        Rectangle
        {
            anchors.fill: parent
            visible: false
            color: "#ff3b30"
        }
    }

    ListView
    {
        id: windowTabs
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: Maui.Style.space.tiny
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds
        model: tabModel

        delegate: Maui.TabButton
        {
            property var windowData: model
            font.pointSize: Maui.Style.fontSizes.small
            implicitHeight: windowTitle.tabHeight
            height: windowTitle.tabHeight
            width: windowTitle.preferredTabWidth(windowData)
            padding: 0
            leftPadding: Maui.Style.space.small
            rightPadding: Maui.Style.space.small
            topPadding: 0
            bottomPadding: 0
            closeButtonVisible: false
            checkable: false
            checked: Boolean(windowData.focused)
            text: windowTitle.cleanText(windowData.title) || i18n("Untitled window")
            icon.name: windowData.iconName || "application-x-executable"

            rightContent: ToolButton
            {
                id: workspaceMenuButton
                icon.name: "application-menu"
                display: ToolButton.IconOnly
                implicitWidth: Maui.Style.iconSizes.small
                implicitHeight: Maui.Style.iconSizes.small
                padding: 0
                enabled: Boolean(windowTitle.bridge && windowTitle.bridge.workspaceCount > 1)
                ToolTip.text: i18n("Move to workspace")
                ToolTip.visible: hovered

                onClicked:
                {
                    windowTitle.toggleWorkspacePopup(workspaceMenuButton, windowData.address)
                }
            }

            onClicked:
            {
                if (windowTitle.bridge)
                    windowTitle.bridge.focusWindow(windowData.address)
            }
        }
    }
}
