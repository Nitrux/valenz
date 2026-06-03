import QtQuick
import QtQuick.Controls
import QtQuick.Controls as QQC

import org.mauikit.controls as Maui

Maui.SettingsDialog
{
    id: control

    property QtObject bridge
    property var _diskUsageOptionsModel: []
    property var _iconModeOptionsModel: [
        {"value": "system16", "display": "System Icons"},
        {"value": "nerd", "display": "Glyph Icons"}
    ]

    onAboutToShow:
    {
        _latitudeField.text = control._formattedCoordinate(control.bridge ? control.bridge.weatherLatitude : 0)
        _longitudeField.text = control._formattedCoordinate(control.bridge ? control.bridge.weatherLongitude : 0)
        _unitCombo.currentIndex = control.bridge && control.bridge.weatherTemperatureUnit === "fahrenheit" ? 1 : 0
        _refreshSpin.value = control.bridge ? control.bridge.weatherRefreshMinutes : 20
        _powerCommandField.text = control.bridge ? control.bridge.controlCenterPowerCommand : "wlogout"
        _refreshDiskUsageOptions()
        _syncDiskUsagePathCombo()
        _syncIconModeCombo()
    }

    function _formattedCoordinate(value)
    {
        const numberValue = Number(value)
        if (!isFinite(numberValue))
            return ""

        return numberValue.toFixed(4)
    }

    function _applyLatitude(text)
    {
        if (!bridge)
            return

        const parsed = Number(text)
        if (!isFinite(parsed))
            return

        bridge.weatherLatitude = parsed
    }

    function _applyLongitude(text)
    {
        if (!bridge)
            return

        const parsed = Number(text)
        if (!isFinite(parsed))
            return

        bridge.weatherLongitude = parsed
    }

    function _applyPowerCommand(text)
    {
        if (!bridge)
            return

        const command = String(text).trim()
        bridge.controlCenterPowerCommand = command.length > 0 ? command : "wlogout"
    }

    function _syncIconModeCombo()
    {
        if (!control.bridge)
            return

        const mode = String(control.bridge.controlCenterIconMode || "system16").trim().toLowerCase()
        for (let i = 0; i < _iconModeOptionsModel.length; ++i) {
            if ((_iconModeOptionsModel[i].value || "auto") === mode) {
                _iconModeCombo.currentIndex = i
                return
            }
        }

        if (_iconModeOptionsModel.length > 0)
            _iconModeCombo.currentIndex = 0
    }

    function _applyIconMode(mode)
    {
        if (!bridge)
            return

        const value = String(mode || "system16").trim().toLowerCase()
        bridge.controlCenterIconMode = value === "nerd" ? "nerd" : "system16"
    }

    function _refreshDiskUsageOptions()
    {
        _diskUsageOptionsModel = control.bridge ? control.bridge.controlCenterDiskUsageOptions() : []
    }

    function _syncDiskUsagePathCombo()
    {
        if (!control.bridge)
            return

        const options = _diskUsageOptionsModel
        const currentPath = control.bridge.controlCenterDiskUsagePath || "/"
        for (let i = 0; i < options.length; ++i) {
            if ((options[i].path || "/") === currentPath) {
                _diskUsageCombo.currentIndex = i
                return
            }
        }

        if (options.length > 0)
            _diskUsageCombo.currentIndex = 0
    }

    function _applyDiskUsagePath(path)
    {
        if (!bridge)
            return

        bridge.controlCenterDiskUsagePath = String(path || "/")
    }

    Maui.SectionGroup
    {
        title: "Modules"

        Maui.FlexSectionItem
        {
            label1.text: "Always Show MPRIS"
            label2.text: "Show media controls even when no player source is detected."

            Switch
            {
                checked: control.bridge ? control.bridge.mprisAlwaysVisible : false
                onToggled:
                {
                    if (control.bridge)
                        control.bridge.mprisAlwaysVisible = checked
                }
            }
        }
    }

    Maui.SectionGroup
    {
        title: "Control Center"

        Maui.FlexSectionItem
        {
            label1.text: "Icon Mode"
            label2.text: "Choose whether Control Center buttons use system icons or glyphs."

            QQC.ComboBox
            {
                id: _iconModeCombo
                readonly property string _fallbackLabel: "System Icons"
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 11, 260)
                currentIndex: -1
                model: _iconModeOptionsModel
                textRole: "display"
                valueRole: "value"
                displayText: currentIndex === -1 ? _fallbackLabel : currentText
                delegate: QQC.ItemDelegate
                {
                    required property var modelData
                    width: ListView.view ? ListView.view.width : _iconModeCombo.width
                    text: modelData && modelData.display ? modelData.display : (modelData && modelData.value ? modelData.value : "")
                }
                popup.width:
                {
                    const popupContentWidth = popup.contentItem ? popup.contentItem.implicitWidth : 0
                    const popupFrameWidth = popup.leftPadding + popup.rightPadding
                    return Math.max(_iconModeCombo.width, popupContentWidth + popupFrameWidth)
                }
                Component.onCompleted:
                {
                    if (control.bridge)
                        control._syncIconModeCombo()
                }
                Connections
                {
                    target: control.bridge
                    enabled: !!control.bridge

                    function onControlCenterIconModeChanged()
                    {
                        control._syncIconModeCombo()
                    }
                }
                onActivated:
                {
                    if (currentIndex >= 0)
                        control._applyIconMode(currentValue)
                }
            }
        }

        Maui.FlexSectionItem
        {
            label1.text: "Power Command"
            label2.text: "Command executed when the Power button is pressed. Defaults to wlogout."

            Maui.TextField
            {
                id: _powerCommandField
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 11, 260)
                text: control.bridge ? control.bridge.controlCenterPowerCommand : "wlogout"
                placeholderText: "wlogout"
                selectByMouse: true
                onEditingFinished: control._applyPowerCommand(text)
            }
        }

        Maui.FlexSectionItem
        {
            label1.text: "Disk Usage Partition"
            label2.text: "Choose the mounted partition the system resources card should monitor."

            QQC.ComboBox
            {
                id: _diskUsageCombo
                readonly property string _fallbackLabel: "Select Partition"
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 11, 260)
                currentIndex: -1
                enabled: _diskUsageOptionsModel.length > 0
                model: _diskUsageOptionsModel
                textRole: "display"
                valueRole: "path"
                displayText: currentIndex === -1 ? _fallbackLabel : currentText
                delegate: QQC.ItemDelegate
                {
                    required property var modelData
                    width: ListView.view ? ListView.view.width : _diskUsageCombo.width
                    text: modelData && modelData.display ? modelData.display : (modelData && modelData.path ? modelData.path : "")
                }
                popup.width:
                {
                    const popupContentWidth = popup.contentItem ? popup.contentItem.implicitWidth : 0
                    const popupFrameWidth = popup.leftPadding + popup.rightPadding
                    return Math.max(_diskUsageCombo.width, popupContentWidth + popupFrameWidth)
                }
                Component.onCompleted:
                {
                    if (control.bridge)
                        control._syncDiskUsagePathCombo()
                }
                Connections
                {
                    target: control.bridge
                    enabled: !!control.bridge

                    function onControlCenterDiskUsagePathChanged()
                    {
                        control._syncDiskUsagePathCombo()
                    }
                }
                onActivated:
                {
                    if (currentIndex >= 0)
                        control._applyDiskUsagePath(currentValue)
                }
            }
        }
    }

    Maui.SectionGroup
    {
        title: "Weather"

        Maui.FlexSectionItem
        {
            label1.text: "Latitude"
            label2.text: "Range: -90.0000 to 90.0000"

            Maui.TextField
            {
                id: _latitudeField
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 8, 180)
                text: control._formattedCoordinate(control.bridge ? control.bridge.weatherLatitude : 0)
                placeholderText: "0.0000"
                selectByMouse: true
                validator: DoubleValidator
                {
                    bottom: -90.0
                    top: 90.0
                    decimals: 4
                }
                onEditingFinished: control._applyLatitude(text)
            }
        }

        Maui.FlexSectionItem
        {
            label1.text: "Longitude"
            label2.text: "Range: -180.0000 to 180.0000"

            Maui.TextField
            {
                id: _longitudeField
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 8, 180)
                text: control._formattedCoordinate(control.bridge ? control.bridge.weatherLongitude : 0)
                placeholderText: "0.0000"
                selectByMouse: true
                validator: DoubleValidator
                {
                    bottom: -180.0
                    top: 180.0
                    decimals: 4
                }
                onEditingFinished: control._applyLongitude(text)
            }
        }

        Maui.FlexSectionItem
        {
            label1.text: "Temperature Unit"
            label2.text: "Select output units for weather values."

            QQC.ComboBox
            {
                id: _unitCombo
                readonly property string _fallbackLabel: "Celsius"
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 7, 160)
                currentIndex: -1
                model: ["Celsius", "Fahrenheit"]
                displayText: currentIndex === -1 ? _fallbackLabel : currentText
                delegate: QQC.ItemDelegate
                {
                    required property string modelData
                    width: ListView.view ? ListView.view.width : _unitCombo.width
                    text: modelData
                }
                popup.width:
                {
                    const popupContentWidth = popup.contentItem ? popup.contentItem.implicitWidth : 0
                    const popupFrameWidth = popup.leftPadding + popup.rightPadding
                    return Math.max(_unitCombo.width, popupContentWidth + popupFrameWidth)
                }
                Component.onCompleted:
                {
                    if (control.bridge)
                        _unitCombo.currentIndex = control.bridge.weatherTemperatureUnit === "fahrenheit" ? 1 : 0
                }
                Connections
                {
                    target: control.bridge
                    enabled: !!control.bridge

                    function onWeatherTemperatureUnitChanged()
                    {
                        _unitCombo.currentIndex = control.bridge.weatherTemperatureUnit === "fahrenheit" ? 1 : 0
                    }
                }
                onActivated:
                {
                    if (!control.bridge)
                        return

                    control.bridge.weatherTemperatureUnit = currentIndex === 1 ? "fahrenheit" : "celsius"
                }
            }
        }

        Maui.FlexSectionItem
        {
            label1.text: "Refresh Interval"
            label2.text: "Automatic refresh period in minutes."

            SpinBox
            {
                id: _refreshSpin
                from: 5
                to: 180
                editable: true
                value: control.bridge ? control.bridge.weatherRefreshMinutes : 20
                onValueModified:
                {
                    if (control.bridge)
                        control.bridge.weatherRefreshMinutes = value
                }
            }
        }
    }
}
