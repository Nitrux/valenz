import QtQuick
import QtQuick.Controls

import org.mauikit.controls as Maui

Item
{
    id: windowTitle

    property QtObject bridge
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
        return Math.max(minimumTabWidth, Math.min(240, textWidth + Maui.Style.iconSizes.small + (Maui.Style.space.big * 2)))
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

    function cleanText(value)
    {
        return String(value || "").trim()
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
        model: windowTitle.windows

        delegate: Maui.TabButton
        {
            id: tabButton
            property var windowData: modelData
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

            contentItem: MouseArea
            {
                implicitWidth: titleContent.implicitWidth
                implicitHeight: titleContent.implicitHeight
                acceptedButtons: Qt.RightButton
                propagateComposedEvents: true
                preventStealing: false

                onClicked: (mouse) =>
                {
                    if (mouse.button === Qt.RightButton)
                        tabButton.rightClicked(mouse)

                    mouse.accepted = false
                }

                Maui.IconLabel
                {
                    id: titleContent
                    anchors.fill: parent
                    text: tabButton.text
                    icon: tabButton.icon
                    color: tabButton.Maui.Theme.textColor
                    alignment: Qt.AlignLeft
                    display: ToolButton.TextBesideIcon
                    font: tabButton.font
                    opacity: tabButton.checked || tabButton.hovered ? 1 : 0.7
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
