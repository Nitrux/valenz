import QtQuick
import QtQuick.Controls

import org.mauikit.controls as Maui

Maui.SettingsDialog
{
    id: control

    property QtObject bridge

    onAboutToShow:
    {
        _latitudeField.text = control._formattedCoordinate(control.bridge ? control.bridge.weatherLatitude : 0)
        _longitudeField.text = control._formattedCoordinate(control.bridge ? control.bridge.weatherLongitude : 0)
        _unitCombo.currentIndex = control.bridge && control.bridge.weatherTemperatureUnit === "fahrenheit" ? 1 : 0
        _refreshSpin.value = control.bridge ? control.bridge.weatherRefreshMinutes : 20
        _powerCommandField.text = control.bridge ? control.bridge.controlCenterPowerCommand : "wlogout"
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

            ComboBox
            {
                id: _unitCombo
                implicitWidth: Math.max(Maui.Style.units.gridUnit * 7, 160)
                model: ["Celsius", "Fahrenheit"]
                currentIndex: control.bridge && control.bridge.weatherTemperatureUnit === "fahrenheit" ? 1 : 0
                onActivated: (index) =>
                {
                    if (!control.bridge)
                        return

                    control.bridge.weatherTemperatureUnit = index === 1 ? "fahrenheit" : "celsius"
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
