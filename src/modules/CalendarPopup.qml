import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

import org.mauikit.controls as Maui
import org.mauikit.calendar as Kalendar

Window
{

    id: calendarPopup

    property Item anchorItem
    property QtObject rootWindow
    property QtObject bridge
    property Item overlayItem
    property date selectedDate: new Date()
    property int displayMonth: selectedDate.getMonth()
    property int displayYear: selectedDate.getFullYear()
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property int _geometryRevision: 0
    property bool _calendarStateSyncing: false
    property bool _fadeOutPending: false
    property bool _panelOpen: false

    readonly property int _fadeInDurationMs: 25
    readonly property int _fadeOutDurationMs: 100

    readonly property int _baseUnit: Math.max(20, Maui.Style.units.gridUnit)
    readonly property int _margin: Math.max(Maui.Style.contentMargins, Maui.Style.space.medium)
    readonly property int _dropOffset: 6
    readonly property int _panelInsetX: 8
    readonly property int _panelInsetY: 8
    readonly property color _panelColor: Maui.Theme.backgroundColor
    readonly property int _preferredPanelWidth: Maui.Handy.isMobile ? _baseUnit * 16 : _baseUnit * 18
    readonly property int _minPanelHeight: Maui.Handy.isMobile ? _baseUnit * 19 : _baseUnit * 20
    readonly property int _preferredPanelHeight: Maui.Handy.isMobile ? _baseUnit * 23 : _baseUnit * 24
    readonly property int _calendarSpacing: Maui.Style.space.small
    readonly property int _eventsCount: _eventCountForSelectedDate()
    readonly property bool _agendaInstalled: !!calendarPopup.bridge && calendarPopup.bridge.agendaInstalled
    readonly property int _effectiveMinPanelHeight: _minPanelHeight

    readonly property real _targetY:
    {
        let targetY = _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return targetY
    }

    readonly property real _availableHeightFromAnchor:
    {
        const screenGeometry = calendarPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.height <= 0)
            return _preferredPanelHeight

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        const startY = Math.max(minY, targetY)
        return Math.max(0, screenGeometry.height - startY - _margin)
    }

    Maui.Theme.colorSet: Maui.Theme.View

    Component.onCompleted: { }
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Popup
    transientParent: rootWindow

    Keys.onEscapePressed:
    {
        close()
    }

    onClosing: function(closeEvent)
    {
        if (_fadeOutPending)
            return

        closeEvent.accepted = false
        close()
    }



    function _touchGeometryRevision()
    {
        _geometryRevision += 1
    }

    function _weekdayLabelsShort()
    {
        const locale = Qt.locale()
        const labels = []
        const mondayBaseDate = new Date(2026, 0, 5)
        const firstDay = Math.max(1, Math.min(7, locale.firstDayOfWeek))

        for (let i = 0; i < 7; ++i)
        {
            const dayOffset = ((firstDay - 1) + i) % 7
            const dayDate = new Date(mondayBaseDate)
            dayDate.setDate(mondayBaseDate.getDate() + dayOffset)
            labels.push(Qt.formatDate(dayDate, "ddd").toLowerCase())
        }

        return labels
    }

    function _syncModelFromDisplay()
    {
        if (!_calendarMonthModel || _calendarStateSyncing)
            return

        _calendarStateSyncing = true
        _calendarMonthModel.year = displayYear
        _calendarMonthModel.month = displayMonth + 1
        _calendarStateSyncing = false
    }

    function _syncDisplayFromModel()
    {
        if (!_calendarMonthModel || _calendarStateSyncing)
            return

        _calendarStateSyncing = true
        displayYear = _calendarMonthModel.year
        displayMonth = Math.max(0, _calendarMonthModel.month - 1)
        _calendarStateSyncing = false
    }

    function _syncSelectionFromDate(dateValue)
    {
        if (!_calendarMonthModel || _calendarStateSyncing)
            return

        _calendarStateSyncing = true
        _calendarMonthModel.selected = dateValue
        _calendarStateSyncing = false
    }

    function previousMonth()
    {
        if (displayMonth === 0)
        {
            displayMonth = 11
            displayYear -= 1
        }
        else
        {
            displayMonth -= 1
        }
    }

    function nextMonth()
    {
        if (displayMonth === 11)
        {
            displayMonth = 0
            displayYear += 1
        }
        else
        {
            displayMonth += 1
        }
    }

    function _dateKey(dateValue)
    {
        if (!dateValue)
            return ""

        return Qt.formatDate(dateValue, "yyyy-MM-dd")
    }

    function _eventCountForSelectedDate()
    {
        const selectedKey = _dateKey(selectedDate)
        if (!selectedKey)
            return 0

        let count = 0

        for (let i = 0; i < _eventsModel.count; ++i)
        {
            if (_eventsModel.get(i).dateKey === selectedKey)
                count += 1
        }

        return count
    }

    function toggleFromAnchor()
    {
        if (visible)
        {
            close()
            return
        }

        if (_lastClosedAtMs >= 0 && (Date.now() - _lastClosedAtMs) < reopenGuardMs)
            return

        open()
    }

    function _anchorPointInScreen(offsetX, offsetY)
    {
        if (!anchorItem || !anchorItem.mapToGlobal)
        {
            return null
        }

        const globalPoint = anchorItem.mapToGlobal(offsetX, offsetY)
        if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
        {
            return globalPoint
        }
        return null
    }

    function _screenGeometry()
    {
        const screen = calendarPopup.rootWindow ? calendarPopup.rootWindow.screen : null
        if (screen && screen.availableGeometry && screen.availableGeometry.width > 0 && screen.availableGeometry.height > 0)
            return screen.availableGeometry
        if (screen && screen.geometry && screen.geometry.width > 0 && screen.geometry.height > 0)
            return screen.geometry
        if (screen && screen.width > 0 && screen.height > 0)
            return Qt.rect(0, 0, screen.width, screen.height)
        if (screen && screen.virtualGeometry && screen.virtualGeometry.width > 0 && screen.virtualGeometry.height > 0)
            return screen.virtualGeometry

        return Qt.rect(0, 0, 0, 0)
    }

    onDisplayMonthChanged: _syncModelFromDisplay()
    onDisplayYearChanged: _syncModelFromDisplay()
    onSelectedDateChanged: _syncSelectionFromDate(selectedDate)

    ListModel
    {
        id: _eventsModel
    }

    Kalendar.MonthModel
    {
        id: _calendarMonthModel

        onMonthChanged: calendarPopup._syncDisplayFromModel()
        onYearChanged: calendarPopup._syncDisplayFromModel()
        onSelectedChanged:
        {
            if (!calendarPopup._calendarStateSyncing)
                calendarPopup.selectedDate = selected
        }
    }

    signal aboutToShow()
    signal opened()
    signal closed()

    function open()
    {
        if (visible)
            return
        aboutToShow()
        _fadeOutPending = false
        _panelOpen = false
        visible = true
        requestActivate()
        Qt.callLater(function()
        {
            if (visible)
                _panelOpen = true
        })
        _logPopupGeometry("opened")
    }

    function close()
    {
        if (!visible)
            return

        _fadeOutPending = true
        _panelOpen = false
        _fadeOutTimer.restart()
    }

    function _logPopupGeometry(reason)
    {
    }

    Timer
    {
        id: _deferredGeometryRefreshTimer
        interval: 32
        repeat: false
        onTriggered:
        {
            if (visible)
            {
                _touchGeometryRevision()
                _logPopupGeometry("deferred")
            }
        }
    }

    Timer
    {
        id: _fadeOutTimer
        interval: calendarPopup._fadeOutDurationMs
        repeat: false
        onTriggered:
        {
            if (calendarPopup._fadeOutPending)
            {
                calendarPopup._fadeOutPending = false
                visible = false
            }
        }
    }

    width:
    {
        const screenGeometry = calendarPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return _preferredPanelWidth

        const available = Math.max(0, screenGeometry.width - (_margin * 2))
        if (available <= 0)
            return _preferredPanelWidth

        return Math.min(_preferredPanelWidth, available)
    }

    height:
    {
        const calendarHeight = _calendarCard ? (_calendarCard.implicitHeight + (_panelInsetY * 2)) : _preferredPanelHeight
        const contentHeight = _contentColumn ? (_contentColumn.implicitHeight + (_panelInsetY * 2)) : calendarHeight
        const desiredHeight = _agendaInstalled ? Math.max(calendarHeight, contentHeight) : calendarHeight
        return Math.min(desiredHeight, _availableHeightFromAnchor)
    }

    onVisibleChanged:
    {
        if (visible)
        {
            opened()
            selectedDate = new Date()
            displayMonth = selectedDate.getMonth()
            displayYear = selectedDate.getFullYear()
            _syncModelFromDisplay()
            _syncSelectionFromDate(selectedDate)
            _touchGeometryRevision()
            Qt.callLater(_touchGeometryRevision)
            _deferredGeometryRefreshTimer.restart()
            _logPopupGeometry("visible")
        }
        else
        {
            _fadeOutTimer.stop()
            _panelOpen = false
            closed()
            _lastClosedAtMs = Date.now()
        }
    }


    x:
    {
        const dep = _geometryRevision
        const screenGeometry = calendarPopup._screenGeometry()
        if (!screenGeometry || screenGeometry.width <= 0)
            return 0

        let targetX = _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetX = p.x - (width / 2)
        }

        const minX = _margin
        const maxX = Math.max(minX, screenGeometry.width - width - _margin)
        const finalX = Math.max(minX, Math.min(maxX, targetX))
        return finalX
    }

    y:
    {
        const dep = _geometryRevision
        const overlay = calendarPopup.overlayItem
        if (!overlay)
            return _margin

        const minY = _margin
        let targetY = Math.max(Maui.Style.toolBarHeightAlt, Maui.Style.units.gridUnit * 2) + _margin
        if (anchorItem)
        {
            const p = _anchorPointInScreen(0, 0)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        const finalY = Math.max(minY, targetY)
        return finalY
    }

    Connections
    {
        target: calendarPopup.anchorItem

        function onXChanged() { calendarPopup._touchGeometryRevision() }
        function onYChanged() { calendarPopup._touchGeometryRevision() }
        function onWidthChanged() { calendarPopup._touchGeometryRevision() }
        function onHeightChanged() { calendarPopup._touchGeometryRevision() }
        function onVisibleChanged() { calendarPopup._touchGeometryRevision() }
    }

    Connections
    {
        target: calendarPopup.overlayItem

        function onWidthChanged() { calendarPopup._touchGeometryRevision() }
        function onHeightChanged() { calendarPopup._touchGeometryRevision() }
        function onXChanged() { calendarPopup._touchGeometryRevision() }
        function onYChanged() { calendarPopup._touchGeometryRevision() }
    }

    Connections
    {
        target: calendarPopup.rootWindow

        function onWidthChanged() { calendarPopup._touchGeometryRevision() }
        function onHeightChanged() { calendarPopup._touchGeometryRevision() }
        function onVisibilityChanged() { calendarPopup._touchGeometryRevision() }
        function onWindowStateChanged() { calendarPopup._touchGeometryRevision() }
    }

    Rectangle
    {
        id: _panel
        anchors.fill: parent
        opacity: 0.0
        scale: 0.97
        transformOrigin: Item.Center
        implicitWidth: calendarPopup.width
        implicitHeight: _contentColumn.implicitHeight + (calendarPopup._panelInsetY * 2)
        radius: Maui.Style.radiusV
        color: calendarPopup._panelColor
        states: [
            State
            {
                name: "open"
                when: calendarPopup._panelOpen
                PropertyChanges
                {
                    target: _panel
                    opacity: 1.0
                    scale: 1.0
                }
            },
            State
            {
                name: "closed"
                when: !calendarPopup._panelOpen
                PropertyChanges
                {
                    target: _panel
                    opacity: 0.0
                    scale: 0.97
                }
            }
        ]
        transitions: Transition
        {
            reversible: true
            NumberAnimation
            {
                properties: "opacity,scale"
                duration: calendarPopup._panelOpen ? calendarPopup._fadeInDurationMs : calendarPopup._fadeOutDurationMs
                easing.type: Easing.InOutCubic
            }
        }
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect
        {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: "#80000000"
        }

        Flickable
        {
            id: _contentFlick
            anchors.fill: parent
            anchors.leftMargin: calendarPopup._panelInsetX
            anchors.rightMargin: calendarPopup._panelInsetX
            anchors.topMargin: calendarPopup._panelInsetY
            anchors.bottomMargin: calendarPopup._panelInsetY
            contentWidth: width
            contentHeight: _contentColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            Column
            {
                id: _contentColumn
                width: _contentFlick.width
                spacing: Maui.Style.space.small

                Maui.SectionItem
                {
                    id: _calendarCard
                    width: parent.width
                    flat: false
                    padding: Maui.Style.space.medium
                    text: ""
                    label2.text: ""

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: calendarPopup._calendarSpacing

                        RowLayout
                        {
                            Layout.fillWidth: true

                            ToolButton
                            {
                                display: ToolButton.IconOnly
                                icon.name: "go-previous"
                                onClicked: calendarPopup.previousMonth()
                            }

                            Label
                            {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                font.weight: Font.DemiBold
                                text: Qt.formatDate(new Date(calendarPopup.displayYear, calendarPopup.displayMonth, 1), "MMMM yyyy")
                            }

                            ToolButton
                            {
                                display: ToolButton.IconOnly
                                icon.name: "go-next"
                                onClicked: calendarPopup.nextMonth()
                            }
                        }

                        GridLayout
                        {
                            Layout.fillWidth: true
                            columns: 7
                            rowSpacing: Maui.Style.space.tiny
                            columnSpacing: Maui.Style.space.small

                            Repeater
                            {
                                model: calendarPopup._weekdayLabelsShort()

                                delegate: Label
                                {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    font.weight: Font.DemiBold
                                    color: Qt.alpha(Maui.Theme.textColor, 0.95)
                                    text: modelData
                                }
                            }
                        }

                        GridLayout
                        {
                            Layout.fillWidth: true
                            columns: 7
                            rowSpacing: Maui.Style.space.tiny
                            columnSpacing: Maui.Style.space.small

                            Repeater
                            {
                                model: _calendarMonthModel

                                delegate: ToolButton
                                {
                                    required property bool sameMonth
                                    required property date date
                                    required property int dayNumber
                                    required property bool isToday
                                    required property bool isSelected

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(32, Maui.Style.rowHeight - Maui.Style.space.tiny)
                                    display: ToolButton.TextOnly
                                    text: String(dayNumber)
                                    opacity: sameMonth ? 1 : 0.18
                                    enabled: sameMonth
                                    font.weight: (isToday || isSelected) ? Font.DemiBold : Font.Normal
                                    onClicked:
                                    {
                                        calendarPopup.selectedDate = date
                                        _calendarMonthModel.selected = date
                                    }

                                    background: Rectangle
                                    {
                                        radius: Maui.Style.radiusV
                                        color: isSelected ? Qt.alpha(Maui.Theme.highlightColor, 0.32)
                                                                    : parent.down || parent.hovered ? Qt.alpha(Maui.Theme.hoverColor, 0.65)
                                                                                                   : "transparent"
                                    }
                                }
                            }
                        }
                    }
                }
                Maui.SectionItem
                {
                    id: _eventsCard
                    visible: calendarPopup._agendaInstalled
                    width: parent.width
                    flat: false
                    padding: Maui.Style.space.medium
                    text: ""
                    label2.text: ""

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: Maui.Style.space.small

                        RowLayout
                        {
                            Layout.fillWidth: true
                            ColumnLayout
                            {
                                spacing: 1

                                Label
                                {
                                    text: "Events"
                                    font.weight: Font.DemiBold
                                }

                                Label
                                {
                                    text: Qt.formatDate(calendarPopup.selectedDate, "ddd, MMM d")
                                    color: Qt.alpha(Maui.Theme.textColor, 0.72)
                                }
                            }

                            Item
                            {
                                Layout.fillWidth: true
                            }

                            Label
                            {
                                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                                text: String(calendarPopup._eventsCount) + (calendarPopup._eventsCount === 1 ? " event" : " events")
                                color: Qt.alpha(Maui.Theme.textColor, 0.72)
                            }
                        }
                        Item
                        {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Maui.Style.space.medium
                        }

                        Label
                        {
                            Layout.fillWidth: true
                            visible: calendarPopup._eventsCount === 0
                            text: "No events scheduled for this day."
                            color: Qt.alpha(Maui.Theme.textColor, 0.72)
                        }

                        Repeater
                        {
                            model: _eventsModel

                            delegate: RowLayout
                            {
                                required property string dateKey
                                required property string timeText
                                required property string titleText
                                required property string detailsText

                                readonly property bool _isForSelectedDate: dateKey === calendarPopup._dateKey(calendarPopup.selectedDate)

                                Layout.fillWidth: true
                                Layout.topMargin: _isForSelectedDate ? Maui.Style.space.small : 0
                                Layout.bottomMargin: _isForSelectedDate ? Maui.Style.space.tiny : 0
                                spacing: Maui.Style.space.small
                                visible: _isForSelectedDate
                                height: _isForSelectedDate ? implicitHeight : 0

                                Label
                                {
                                    Layout.alignment: Qt.AlignTop
                                    text: timeText
                                    width: Math.max(52, implicitWidth)
                                    horizontalAlignment: Text.AlignRight
                                    color: Qt.alpha(Maui.Theme.textColor, 0.72)
                                }

                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label
                                    {
                                        Layout.fillWidth: true
                                        text: titleText
                                        elide: Text.ElideRight
                                        font.weight: Font.Medium
                                    }

                                    Label
                                    {
                                        Layout.fillWidth: true
                                        text: detailsText
                                        elide: Text.ElideRight
                                        color: Qt.alpha(Maui.Theme.textColor, 0.72)
                                    }
                                }
                            }
                        }

                        RowLayout
                        {
                            Layout.fillWidth: true

                            Item
                            {
                                Layout.fillWidth: true
                            }

                            Button
                            {
                                visible: calendarPopup._agendaInstalled
                                text: "Edit Events"
                                onClicked:
                                {
                                    if (calendarPopup.rootWindow && calendarPopup.rootWindow.traceMenu)
                                        calendarPopup.rootWindow.traceMenu("calendar_open_agenda", Qt.formatDate(calendarPopup.selectedDate, "yyyy-MM-dd"))
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
