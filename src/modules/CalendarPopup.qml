import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import org.mauikit.controls as Maui
import org.mauikit.calendar as Kalendar

Dialog
{
    id: calendarPopup

    property Item anchorItem
    property QtObject rootWindow
    property QtObject bridge
    property Item overlayItem: calendarPopup.parent
    property date selectedDate: new Date()
    property int displayMonth: selectedDate.getMonth()
    property int displayYear: selectedDate.getFullYear()
    property int reopenGuardMs: 180
    property double _lastClosedAtMs: -1
    property int _geometryRevision: 0
    property bool _calendarStateSyncing: false

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
    readonly property int _effectiveMinPanelHeight: calendarPopup._agendaInstalled ? _minPanelHeight : 0

    readonly property real _targetY:
    {
        let targetY = _margin
        if (anchorItem)
        {
            const p = _anchorPointInOverlay(0, anchorItem.height)
            if (p)
                targetY = p.y + Maui.Style.space.small + _dropOffset
        }

        return targetY
    }

    readonly property real _availableHeightFromAnchor:
    {
        const overlay = calendarPopup.overlayItem
        if (!overlay)
            return _preferredPanelHeight

        const minY = _margin
        const startY = Math.max(minY, _targetY)
        return Math.max(0, overlay.height - startY - _margin)
    }

    Maui.Theme.colorSet: Maui.Theme.View

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

    function _anchorPointInOverlay(offsetX, offsetY)
    {
        const overlay = calendarPopup.overlayItem
        if (!overlay || !anchorItem)
            return null

        if (anchorItem.mapToGlobal && overlay.mapFromGlobal)
        {
            const globalPoint = anchorItem.mapToGlobal(offsetX, offsetY)
            if (globalPoint && isFinite(globalPoint.x) && isFinite(globalPoint.y))
            {
                const localPoint = overlay.mapFromGlobal(globalPoint.x, globalPoint.y)
                if (localPoint && isFinite(localPoint.x) && isFinite(localPoint.y))
                    return localPoint
            }
        }

        const mappedPoint = anchorItem.mapToItem(overlay, offsetX, offsetY)
        if (mappedPoint && isFinite(mappedPoint.x) && isFinite(mappedPoint.y))
            return mappedPoint

        return null
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

    modal: false
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    transformOrigin: Item.Top

    enter: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 170
                easing.type: Easing.OutCubic
            }

            NumberAnimation
            {
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 170
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 130
                easing.type: Easing.OutCubic
            }

            NumberAnimation
            {
                property: "scale"
                from: 1.0
                to: 0.97
                duration: 130
                easing.type: Easing.OutCubic
            }
        }
    }

    width:
    {
        const overlay = calendarPopup.overlayItem
        if (!overlay)
            return _preferredPanelWidth

        const available = Math.max(0, overlay.width - (_margin * 2))
        if (available <= 0)
            return _preferredPanelWidth

        return Math.min(_preferredPanelWidth, available)
    }

    height:
    {
        const contentHeight = _contentColumn ? (_contentColumn.implicitHeight + (_panelInsetY * 2)) : _preferredPanelHeight
        const desiredHeight = Math.max(_effectiveMinPanelHeight, contentHeight)
        return Math.min(desiredHeight, _availableHeightFromAnchor)
    }

    onAboutToShow:
    {
        selectedDate = new Date()
        displayMonth = selectedDate.getMonth()
        displayYear = selectedDate.getFullYear()
        _syncModelFromDisplay()
        _syncSelectionFromDate(selectedDate)
        _touchGeometryRevision()
    }

    onOpened: Qt.callLater(_touchGeometryRevision)
    onClosed: _lastClosedAtMs = Date.now()

    anchors.centerIn: undefined

    x:
    {
        const dep = _geometryRevision
        const overlay = calendarPopup.overlayItem
        if (!overlay)
            return 0

        let targetX = _margin
        if (anchorItem)
        {
            const p = _anchorPointInOverlay(anchorItem.width / 2, anchorItem.height)
            if (p)
                targetX = p.x - (width / 2)
        }

        const minX = _margin
        const maxX = Math.max(minX, overlay.width - width - _margin)
        return Math.max(minX, Math.min(maxX, targetX))
    }

    y:
    {
        const dep = _geometryRevision
        const overlay = calendarPopup.overlayItem
        if (!overlay)
            return _margin

        const minY = _margin
        return Math.max(minY, _targetY)
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

    background: Rectangle
    {
        color: Qt.alpha(calendarPopup._panelColor, 0)
    }

    contentItem: Rectangle
    {
        id: _panel
        implicitWidth: calendarPopup.width
        implicitHeight: calendarPopup.height
        radius: Maui.Style.radiusV
        color: calendarPopup._panelColor
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
