import QtQuick
import QtQuick.Controls

import org.mauikit.controls as Maui

Item
{
    id: windowTitle

    property QtObject bridge
    property string fallbackTitle: ""
    property int referenceHeight: -1
    readonly property int tabHeight: Math.min(28, referenceHeight > 0 ? referenceHeight : _toolButtonHeightProbe.implicitHeight)
    readonly property string displayTitle:
    {
        const focused = windowTitle.cleanText(windowTitle.bridge ? windowTitle.bridge.focusedWindowTitle : "")
        return focused.length > 0 ? focused : windowTitle.cleanText(windowTitle.fallbackTitle)
    }
    readonly property bool hasTitle: displayTitle.length > 0

    visible: hasTitle

    implicitWidth: hasTitle
                   ? Math.max(
                         Maui.Style.units.gridUnit * 4,
                         _focusedWindowMetrics.advanceWidth + Maui.Style.iconSizes.small + (Maui.Style.space.big * 3))
                   : 0
    implicitHeight: hasTitle ? tabHeight : 0

    function cleanText(value)
    {
        return String(value || "").trim()
    }

    TextMetrics
    {
        id: _focusedWindowMetrics
        text: windowTitle.displayTitle
        font: _focusedWindowText.font
    }

    ToolButton
    {
        id: _toolButtonHeightProbe
        visible: false
        display: ToolButton.IconOnly
        icon.name: "media-playback-start"
    }

    Maui.TabButton
    {
        id: _focusedWindowTab
        font.pointSize: Maui.Style.fontSizes.small
        implicitHeight: windowTitle.tabHeight
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: windowTitle.tabHeight
        padding: 0
        leftPadding: Maui.Style.space.small
        rightPadding: Maui.Style.space.small
        topPadding: 0
        bottomPadding: 0
        closeButtonVisible: false
        text: ""
        icon.name: ""
        checkable: false
        checked: false

        leftContent: [
            Item
            {
                implicitWidth: Maui.Style.iconSizes.small + Maui.Style.space.small
                implicitHeight: Maui.Style.iconSizes.small

                Maui.Icon
                {
                    anchors.centerIn: parent
                    width: Maui.Style.iconSizes.small
                    height: width
                    source: windowTitle.bridge ? windowTitle.bridge.focusedWindowIconName : "application-x-executable"
                    color: Maui.Theme.textColor
                }
            }
        ]

        Item
        {
            id: _focusedWindowTextWrap
            anchors.fill: parent
            anchors.leftMargin: Maui.Style.space.tiny
            anchors.rightMargin: Maui.Style.space.tiny
            clip: true
            property bool overflow: _focusedWindowText.implicitWidth > _focusedWindowViewport.width
            property bool scrolling: _focusedWindowHover.hovered && overflow
            property real scrollDistance: Math.max(0, _focusedWindowText.implicitWidth - _focusedWindowViewport.width + (Maui.Style.space.medium * 2))

            HoverHandler
            {
                id: _focusedWindowHover
            }

            onScrollingChanged:
            {
                _focusedWindowText.x = 0
                if (scrolling)
                    _focusedWindowHoverMarquee.restart()
                else
                    _focusedWindowHoverMarquee.stop()
            }

            Item
            {
                id: _focusedWindowViewport
                anchors.fill: parent
                clip: true

                Label
                {
                    id: _focusedWindowText
                    y: (_focusedWindowViewport.height - height) / 2
                    text: windowTitle.displayTitle
                    color: Maui.Theme.textColor
                    wrapMode: Text.NoWrap
                    width: _focusedWindowTextWrap.scrolling ? implicitWidth : _focusedWindowViewport.width
                    elide: _focusedWindowTextWrap.scrolling ? Text.ElideNone : Text.ElideRight
                    onTextChanged:
                    {
                        opacity = 0.2
                        _focusedWindowFade.restart()
                    }

                }
            }
        }
    }

    NumberAnimation
    {
        id: _focusedWindowFade
        target: _focusedWindowText
        property: "opacity"
        from: 0.2
        to: 1.0
        duration: 220
        easing.type: Easing.OutCubic
    }

    SequentialAnimation
    {
        id: _focusedWindowHoverMarquee
        loops: Animation.Infinite
        running: false
        PauseAnimation { duration: 250 }
        NumberAnimation
        {
            target: _focusedWindowText
            property: "x"
            to: -_focusedWindowTextWrap.scrollDistance
            duration: Math.max(1800, _focusedWindowTextWrap.scrollDistance * 28)
            easing.type: Easing.Linear
        }
        PauseAnimation { duration: 220 }
        NumberAnimation
        {
            target: _focusedWindowText
            property: "x"
            to: 0
            duration: 220
            easing.type: Easing.OutCubic
        }
    }
}
