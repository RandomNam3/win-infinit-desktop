// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Color swatch button with a picker popover (saturation/value area, hue strip,
// presets and a hex field). Changes are reported live while dragging.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Basic.AbstractButton {
    id: control

    property color value: "#ffffff"
    property string accessibleName

    signal picked(color color)

    function openPicker() {
        picker.open();
    }

    implicitWidth: row.implicitWidth + 20
    implicitHeight: Theme.controlHeight
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName + ": " + value.toString().toUpperCase()

    onClicked: picker.opened ? picker.close() : picker.open()

    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 6
                color: control.value
                border.width: 1
                border.color: Theme.borderStrong
            }
            UiText {
                text: control.value.toString().toUpperCase()
                font.pointSize: Theme.fontBody
                font.features: { "tnum": 1 }
            }
            Icon {
                name: "chevron-down"
                size: 14
                color: Theme.textSecondary
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMedium
        color: control.down ? Theme.controlPressed : control.hovered ? Theme.controlHover : Theme.control

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus
        }
    }

    Popover {
        id: picker

        property real hue: 0
        property real saturation: 0
        property real brightness: 1
        property bool syncing: false

        function syncFromValue() {
            syncing = true;
            if (control.value.hsvHue >= 0) {
                hue = control.value.hsvHue;
            }
            saturation = control.value.hsvSaturation;
            brightness = control.value.hsvValue;
            hexField.text = control.value.toString().toUpperCase();
            syncing = false;
        }

        function emitCurrent() {
            if (syncing) {
                return;
            }
            const c = Qt.hsva(hue, saturation, brightness, 1);
            hexField.text = c.toString().toUpperCase();
            control.picked(c);
        }

        alignment: "end"
        width: 290
        onAboutToShow: syncFromValue()

        contentItem: ColumnLayout {
            spacing: 12

            // Saturation (x) / brightness (y)
            Rectangle {
                id: svArea

                Layout.fillWidth: true
                Layout.preferredHeight: 150
                radius: Theme.radiusMedium
                color: Qt.hsva(picker.hue, 1, 1, 1)

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#ffffffff" }
                        GradientStop { position: 1; color: "#00ffffff" }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00000000" }
                        GradientStop { position: 1; color: "#ff000000" }
                    }
                }
                Rectangle {
                    x: picker.saturation * parent.width - width / 2
                    y: (1 - picker.brightness) * parent.height - height / 2
                    width: 16
                    height: 16
                    radius: 8
                    color: "transparent"
                    border.width: 2
                    border.color: "#ffffff"

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: width / 2
                        color: "transparent"
                        border.width: 1
                        border.color: Qt.rgba(0, 0, 0, 0.35)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    cursorShape: Qt.CrossCursor
                    function pick(mouse) {
                        picker.saturation = Math.max(0, Math.min(1, mouse.x / width));
                        picker.brightness = 1 - Math.max(0, Math.min(1, mouse.y / height));
                        picker.emitCurrent();
                    }
                    onPressed: mouse => pick(mouse)
                    onPositionChanged: mouse => pick(mouse)
                }
            }

            // Hue
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 16

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    radius: height / 2
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#ff0000" }
                        GradientStop { position: 0.167; color: "#ffff00" }
                        GradientStop { position: 0.333; color: "#00ff00" }
                        GradientStop { position: 0.5; color: "#00ffff" }
                        GradientStop { position: 0.667; color: "#0000ff" }
                        GradientStop { position: 0.833; color: "#ff00ff" }
                        GradientStop { position: 1.0; color: "#ff0000" }
                    }
                }
                Rectangle {
                    x: picker.hue * (parent.width - width)
                    width: 16
                    height: 16
                    radius: 8
                    color: Qt.hsva(picker.hue, 1, 1, 1)
                    border.width: 2
                    border.color: "#ffffff"
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    function pick(mouse) {
                        picker.hue = Math.max(0, Math.min(1, mouse.x / width));
                        picker.emitCurrent();
                    }
                    onPressed: mouse => pick(mouse)
                    onPositionChanged: mouse => pick(mouse)
                }
            }

            // Presets
            Flow {
                Layout.fillWidth: true
                spacing: 5

                Repeater {
                    model: ["#ffffff", "#c4c9d1", "#6b7280", "#000000", "#3daee9", "#7c5cf0", "#e0569b", "#f2994a", "#4cc38a"]

                    Basic.AbstractButton {
                        id: preset

                        required property string modelData

                        width: 24
                        height: 24
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus

                        Accessible.role: Accessible.Button
                        Accessible.name: modelData

                        onClicked: {
                            control.picked(modelData);
                            picker.syncFromValue();
                        }

                        background: Rectangle {
                            radius: 6
                            color: preset.modelData
                            border.width: 1
                            border.color: Theme.borderStrong
                            scale: preset.hovered ? 1.1 : 1

                            Behavior on scale {
                                NumberAnimation { duration: Theme.durationFast }
                            }

                            FocusRing {
                                radius: parent.radius + offset
                                visible: preset.visualFocus
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: Theme.controlHeight
                    Layout.preferredHeight: Theme.controlHeight
                    radius: Theme.radiusMedium
                    color: control.value
                    border.width: 1
                    border.color: Theme.borderStrong
                }
                UiTextField {
                    id: hexField

                    Layout.fillWidth: true
                    font.features: { "tnum": 1 }
                    validator: RegularExpressionValidator {
                        regularExpression: /#?[0-9a-fA-F]{0,6}/
                    }
                    Accessible.name: i18nc("@label color as hexadecimal value", "Hex value")
                    onEditingFinished: {
                        let t = text.trim();
                        if (!t.startsWith("#")) {
                            t = "#" + t;
                        }
                        if (/^#[0-9a-fA-F]{6}$/.test(t)) {
                            control.picked(t);
                            picker.syncFromValue();
                        }
                    }
                }
            }
        }
    }
}
