// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Integer field with - and + buttons. Type a value, use the arrow keys or the buttons.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

FocusScope {
    id: root

    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    // Formatted value incl. unit, shown while the field is not edited.
    property string displayText: value.toString()
    property string accessibleName

    signal valueModified(int value)

    function modify(newValue) {
        const clamped = Math.max(from, Math.min(to, newValue));
        if (clamped !== value) {
            valueModified(clamped);
        }
    }

    function commit() {
        let parsed = Number.fromLocaleString(Qt.locale(), input.text);
        if (isNaN(parsed)) {
            parsed = parseInt(input.text.replace(/[^0-9-]/g, ""));
        }
        if (!isNaN(parsed)) {
            modify(Math.round(parsed));
        }
        input.text = Qt.binding(() => root.value.toString());
    }

    implicitWidth: 132
    implicitHeight: Theme.controlHeight
    opacity: enabled ? 1 : 0.45
    activeFocusOnTab: false

    Rectangle {
        id: frame

        anchors.fill: parent
        radius: Theme.radiusMedium
        color: input.activeFocus ? Theme.input : Theme.control
        border.width: input.activeFocus ? 1.5 : 0
        border.color: Theme.accentText

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0

            IconButton {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                round: false
                iconName: "minus"
                iconSize: 14
                tooltip: i18nc("@action:button decrease a number", "Decrease")
                enabled: root.value > root.from
                focusPolicy: Qt.NoFocus
                autoRepeat: true
                onClicked: root.modify(root.value - root.stepSize)
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextInput {
                    id: input

                    anchors.fill: parent
                    // Invisible items cannot take focus, so only fade it out.
                    opacity: activeFocus ? 1 : 0
                    text: root.value.toString()
                    color: Theme.text
                    selectionColor: Theme.accentSoft
                    selectedTextColor: Theme.text
                    font.pointSize: Theme.fontBody
                    font.features: { "tnum": 1 }
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                    activeFocusOnTab: true
                    focus: true
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {
                        bottom: Math.min(root.from, 0)
                        top: root.to
                    }

                    Accessible.role: Accessible.SpinBox
                    Accessible.name: root.accessibleName

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            selectAll();
                        } else {
                            root.commit();
                        }
                    }
                    onAccepted: {
                        root.commit();
                        selectAll();
                    }
                    Keys.onUpPressed: root.modify(root.value + root.stepSize)
                    Keys.onDownPressed: root.modify(root.value - root.stepSize)
                    Keys.onEscapePressed: {
                        text = Qt.binding(() => root.value.toString());
                        focus = false;
                    }
                }

                UiText {
                    anchors.fill: parent
                    visible: !input.activeFocus
                    text: root.displayText
                    font.pointSize: Theme.fontBody
                    font.features: { "tnum": 1 }
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    visible: !input.activeFocus
                    cursorShape: Qt.IBeamCursor
                    onClicked: input.forceActiveFocus()
                    onWheel: wheel => {
                        root.modify(root.value + (wheel.angleDelta.y > 0 ? root.stepSize : -root.stepSize));
                    }
                }
            }

            IconButton {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                round: false
                iconName: "plus"
                iconSize: 14
                tooltip: i18nc("@action:button increase a number", "Increase")
                enabled: root.value < root.to
                focusPolicy: Qt.NoFocus
                autoRepeat: true
                onClicked: root.modify(root.value + root.stepSize)
            }
        }
    }
}
