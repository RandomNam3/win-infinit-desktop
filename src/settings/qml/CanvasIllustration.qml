// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Decorative picture of an infinite canvas: a dot grid with windows scattered
// around and the screen as a viewport that glides over it.
import QtQuick

Rectangle {
    id: root

    property bool live: true

    implicitWidth: 280
    implicitHeight: 176
    radius: Theme.radiusLarge
    color: Theme.dark ? "#191c22" : "#f5f7fb"
    border.width: 1
    border.color: Theme.border
    clip: true
    Accessible.ignored: true

    readonly property color windowColor: Theme.dark ? "#2b3038" : "#ffffff"
    readonly property color windowBorder: Theme.dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0.07, 0.09, 0.15, 0.1)

    Item {
        id: canvas

        width: parent.width + 80
        height: parent.height + 60
        x: -40 + 18 * Math.sin(drift.phase)
        y: -30 + 10 * Math.cos(drift.phase * 0.8)

        // Dot grid
        Grid {
            x: 6
            y: 6
            columns: Math.ceil(canvas.width / 18)
            spacing: 16

            Repeater {
                model: Math.ceil(canvas.width / 18) * Math.ceil(canvas.height / 18)

                Rectangle {
                    width: 2
                    height: 2
                    radius: 1
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0.1, 0.15, 0.3, 0.16)
                }
            }
        }

        // Windows on the canvas
        Repeater {
            model: [
                { "x": 26, "y": 34, "w": 84, "h": 56, "accent": false },
                { "x": 128, "y": 18, "w": 104, "h": 70, "accent": true },
                { "x": 250, "y": 46, "w": 70, "h": 48, "accent": false },
                { "x": 64, "y": 114, "w": 96, "h": 60, "accent": false },
                { "x": 182, "y": 112, "w": 82, "h": 54, "accent": false },
                { "x": 286, "y": 122, "w": 60, "h": 44, "accent": false }
            ]

            Rectangle {
                required property var modelData

                x: modelData.x
                y: modelData.y
                width: modelData.w
                height: modelData.h
                radius: 6
                color: root.windowColor
                border.width: 1
                border.color: modelData.accent ? Theme.alpha(Theme.accentText, 0.6) : root.windowBorder

                Rectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 11
                    radius: 5
                    color: modelData.accent ? Theme.accentSoft : (Theme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0.07, 0.09, 0.15, 0.04))
                }
                Row {
                    x: 6
                    y: 4
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 4
                            height: 4
                            radius: 2
                            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0.07, 0.09, 0.15, 0.18)
                        }
                    }
                }
                Column {
                    x: 8
                    y: 20
                    spacing: 5
                    Repeater {
                        model: Math.max(1, Math.floor((parent.parent.height - 26) / 9))
                        Rectangle {
                            required property int index
                            width: (parent.parent.width - 16) * (index % 2 === 0 ? 0.8 : 0.55)
                            height: 3
                            radius: 1.5
                            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.07, 0.09, 0.15, 0.07)
                        }
                    }
                }
            }
        }
    }

    // The screen: a viewport over the canvas
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.56
        height: parent.height * 0.6
        radius: 10
        color: "transparent"
        border.width: 2
        border.color: Theme.accentText

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.alpha(Theme.accent, 0.06)
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
            anchors.topMargin: -1
            width: viewportLabel.implicitWidth + 14
            height: 18
            radius: 5
            color: Theme.accentText

            UiText {
                id: viewportLabel
                anchors.centerIn: parent
                text: "100 %"
                color: Theme.dark ? "#10141a" : "#ffffff"
                font.pointSize: Theme.fontCaption
                font.weight: Font.DemiBold
            }
        }
    }

    QtObject {
        id: drift
        property real phase: 0

        property NumberAnimation animation: NumberAnimation {
            target: drift
            property: "phase"
            from: 0
            to: Math.PI * 2
            duration: 24000
            loops: Animation.Infinite
            running: root.live && Theme.animations && root.visible
        }
    }
}
