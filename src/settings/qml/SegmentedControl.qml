// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row of mutually exclusive options with a sliding selection indicator.
// Keyboard: Tab focuses the control, Left/Right change the selection.
import QtQuick
import QtQuick.Controls.Basic as Basic

FocusScope {
    id: root

    property var model: []
    property int currentIndex: 0
    property string accessibleName

    signal activated(int index)

    readonly property Item currentItem: repeater.count > currentIndex && currentIndex >= 0 ? repeater.itemAt(currentIndex) : null

    implicitWidth: row.implicitWidth + 6
    implicitHeight: Theme.controlHeight
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.Grouping
    Accessible.name: accessibleName

    Keys.onLeftPressed: {
        if (currentIndex > 0) {
            activated(currentIndex - 1);
        }
    }
    Keys.onRightPressed: {
        if (currentIndex < model.length - 1) {
            activated(currentIndex + 1);
        }
    }

    Rectangle {
        id: track

        anchors.fill: parent
        radius: Theme.radiusMedium
        color: Theme.control

        Rectangle {
            id: indicator

            visible: root.currentItem !== null
            x: root.currentItem ? row.x + root.currentItem.x : 0
            y: 3
            width: root.currentItem ? root.currentItem.width : 0
            height: parent.height - 6
            radius: Theme.radiusMedium - 2
            color: Theme.dark ? "#434953" : "#ffffff"
            border.width: Theme.dark ? 0 : 1
            border.color: Theme.border

            Behavior on x {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }

            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.topMargin: 1
                anchors.bottomMargin: -1
                radius: parent.radius
                color: Qt.rgba(0, 0, 0, Theme.dark ? 0.25 : 0.06)
            }
        }

        Row {
            id: row

            x: 3
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Repeater {
                id: repeater

                model: root.model

                Basic.AbstractButton {
                    id: segment

                    required property int index
                    required property var modelData
                    readonly property bool selected: index === root.currentIndex

                    implicitWidth: Math.max(label.implicitWidth + 26, 52)
                    implicitHeight: root.height - 6
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus

                    Accessible.role: Accessible.RadioButton
                    Accessible.checkable: true
                    Accessible.checked: selected
                    Accessible.name: String(modelData)
                    Accessible.onPressAction: root.activated(index)

                    onClicked: root.activated(index)

                    background: Rectangle {
                        radius: Theme.radiusMedium - 2
                        color: segment.hovered && !segment.selected ? Theme.hover : Theme.alpha(Theme.hover, 0)
                    }

                    contentItem: UiText {
                        id: label
                        text: String(segment.modelData)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: segment.selected ? Theme.text : Theme.textSecondary
                        font.weight: Font.Medium
                        font.pointSize: Theme.fontBody

                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast }
                        }
                    }
                }
            }
        }
    }

    FocusRing {
        radius: track.radius + offset
        visible: root.activeFocus
    }
}
