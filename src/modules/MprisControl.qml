import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui

Item
{
    id: mprisControl

    property QtObject bridge
    property var popup
    property Item popupAnchorMarker: _popupAnchorMarker
    readonly property int actionsButtonHeight: _mediaControls.implicitHeight

    Layout.fillWidth: false
    implicitHeight: visible ? Math.max(_artFrame.implicitHeight, _mediaGrid.implicitHeight) : 0
    implicitWidth: visible ? _mediaGrid.implicitWidth : 0

    function cleanText(value)
    {
        return String(value || "").trim()
    }

    function toggleSourcesPopup()
    {
        if (!mprisControl.popup)
            return

        if (mprisControl.popup.visible)
            mprisControl.popup.close()
        else
            mprisControl.popup.open()
    }

    GridLayout
    {
        id: _mediaGrid
        anchors.fill: parent
        columns: 3
        columnSpacing: Maui.Style.space.medium
        rowSpacing: 0

        Rectangle
        {
            id: _artFrame
            Layout.row: 0
            Layout.column: 0
            Layout.rowSpan: 2
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Math.max(Maui.Style.iconSizes.medium, Maui.Style.toolBarHeightAlt - Maui.Style.space.small)
            implicitHeight: Maui.Style.iconSizes.big
            radius: Maui.Style.radiusV
            color: Maui.Theme.alternateBackgroundColor
            clip: true

            Image
            {
                id: _mediaArtImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: (mprisControl.bridge && String(mprisControl.bridge.mediaArtSource || "").trim().length > 0)
                        ? mprisControl.bridge.mediaArtSource
                        : ""
                visible: true
            }

            Label
            {
                anchors.centerIn: parent
                font.pointSize: Maui.Style.fontSizes.big
                text: "\u266B"
                visible: _mediaArtImage.status !== Image.Ready
            }
        }

        Item
        {
            id: _mediaPrimaryTextWrap
            Layout.row: 0
            Layout.column: 1
            Layout.preferredWidth: implicitWidth
            Layout.alignment: Qt.AlignBottom
            implicitWidth: Math.min(Math.max(Maui.Style.units.gridUnit * 5, _mediaPrimaryText.implicitWidth), Maui.Style.units.gridUnit * 8)
            implicitHeight: _mediaPrimaryText.implicitHeight
            clip: true
            property bool overflow: _mediaPrimaryText.implicitWidth > _mediaPrimaryViewport.width
            property bool scrolling: _mediaPrimaryHover.hovered && overflow
            property real scrollDistance: Math.max(0, _mediaPrimaryText.implicitWidth - _mediaPrimaryViewport.width + (Maui.Style.space.medium * 2))

            HoverHandler
            {
                id: _mediaPrimaryHover
            }

            onScrollingChanged:
            {
                _mediaPrimaryText.x = 0
                if (scrolling)
                    _mediaPrimaryHoverMarquee.restart()
                else
                    _mediaPrimaryHoverMarquee.stop()
            }

            Item
            {
                id: _mediaPrimaryViewport
                anchors.fill: parent
                clip: true

                Label
                {
                    id: _mediaPrimaryText
                    font.pointSize: Maui.Style.fontSizes.big
                    y: (_mediaPrimaryTextWrap.height - height) / 2
                    text:
                    {
                        const title = mprisControl.cleanText(mprisControl.bridge ? mprisControl.bridge.mediaTitle : "")
                        const artist = mprisControl.cleanText(mprisControl.bridge ? mprisControl.bridge.mediaArtist : "")
                        const parts = []
                        if (title.length > 0)
                            parts.push(title)
                        if (artist.length > 0)
                            parts.push(artist)
                        return parts.length > 0 ? parts.join(" - ") : "No media"
                    }
                    font.weight: Font.DemiBold
                    color: Maui.Theme.textColor
                    wrapMode: Text.NoWrap
                    width: _mediaPrimaryTextWrap.scrolling ? implicitWidth : _mediaPrimaryViewport.width
                    elide: _mediaPrimaryTextWrap.scrolling ? Text.ElideNone : Text.ElideRight
                    onTextChanged:
                    {
                        opacity = 0.2
                        _mediaPrimaryFade.restart()
                    }

                    ToolTip.delay: 500
                    ToolTip.visible: _mediaPrimaryHover.hovered && _mediaPrimaryTextWrap.overflow
                    ToolTip.text: text
                }
            }

            NumberAnimation
            {
                id: _mediaPrimaryFade
                target: _mediaPrimaryText
                property: "opacity"
                from: 0.2
                to: 1.0
                duration: 220
                easing.type: Easing.OutCubic
            }

            SequentialAnimation
            {
                id: _mediaPrimaryHoverMarquee
                loops: Animation.Infinite
                running: false
                PauseAnimation { duration: 250 }
                NumberAnimation
                {
                    target: _mediaPrimaryText
                    property: "x"
                    to: -_mediaPrimaryTextWrap.scrollDistance
                    duration: Math.max(1800, _mediaPrimaryTextWrap.scrollDistance * 28)
                    easing.type: Easing.Linear
                }
                PauseAnimation { duration: 220 }
                NumberAnimation
                {
                    target: _mediaPrimaryText
                    property: "x"
                    to: 0
                    duration: 220
                    easing.type: Easing.OutCubic
                }
            }
        }

        Item
        {
            id: _mediaSecondaryTextWrap
            Layout.row: 1
            Layout.column: 1
            Layout.preferredWidth: _mediaPrimaryTextWrap.implicitWidth
            Layout.alignment: Qt.AlignTop
            implicitHeight: _mediaSecondaryText.implicitHeight
            clip: true

            Item
            {
                id: _mediaSecondaryViewport
                anchors.fill: parent
                clip: true
                Label
                {
                    id: _mediaSecondaryText
                    font.pointSize: Maui.Style.fontSizes.tiny
                    y: (_mediaSecondaryTextWrap.height - height) / 2
                    color: Maui.Theme.textColor
                    text:
                    {
                        const timestamp = mprisControl.cleanText(mprisControl.bridge ? mprisControl.bridge.mediaTimestamp : "")
                        return timestamp.length > 0 ? timestamp : "--:-- / --:--"
                    }
                    wrapMode: Text.NoWrap
                    width: _mediaSecondaryViewport.width
                    elide: Text.ElideRight
                }
            }
        }

        Maui.ToolActions
        {
            id: _mediaControls
            Layout.row: 0
            Layout.column: 2
            Layout.rowSpan: 2
            Layout.alignment: Qt.AlignVCenter
            display: ToolButton.IconOnly
            checkable: false
            autoExclusive: false
            expanded: true

            Action
            {
                text: i18n("Media sources")
                icon.name: "open-menu-symbolic"
                onTriggered: mprisControl.toggleSourcesPopup()
            }

            Action
            {
                text: i18n("Previous track")
                icon.name: "media-skip-backward"
                onTriggered: mprisControl.bridge.mediaPreviousTrack()
            }

            Action
            {
                text: i18n("Play and pause")
                icon.name: mprisControl.bridge && mprisControl.bridge.mediaPlaying ? "media-playback-pause" : "media-playback-start"
                onTriggered: mprisControl.bridge.mediaTogglePlayPause()
            }

            Action
            {
                text: i18n("Next track")
                icon.name: "media-skip-forward"
                onTriggered: mprisControl.bridge.mediaNextTrack()
            }
        }
    }

    Item
    {
        id: _popupAnchorMarker
        x: _mediaControls.x + (_mediaControls.width / 2)
        y: parent.height
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
