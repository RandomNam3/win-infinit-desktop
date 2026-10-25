// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The "i" next to a setting. Opens a popover with the full explanation,
// the default value and the allowed range.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Basic.AbstractButton {
    id: control

    property string title
    property string explanation
    property string defaultText
    property string rangeText
    property bool isDefault: true

    signal resetRequested()

    function openPopover() {
        popover.open();
    }

    implicitWidth: 22
    implicitHeight: 22
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: i18nc("@action:button %1 is the name of a setting", "More information about “%1”", title)
    Accessible.description: explanation

    onClicked: popover.opened ? popover.close() : popover.open()

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: "info"
            size: 16
            color: control.hovered || popover.opened || control.visualFocus ? Theme.accentText : Theme.textTertiary
        }
    }

    background: Rectangle {
        radius: width / 2
        color: control.hovered || popover.opened ? Theme.accentSofter : Theme.alpha(Theme.accentSofter, 0)

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus
        }
    }

    Popover {
        id: popover

        width: Math.min(340, (Basic.Overlay.overlay ? Basic.Overlay.overlay.width : 400) - 24)

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: 14
                    color: Theme.accentSoft

                    Icon {
                        anchors.centerIn: parent
                        name: "info"
                        size: 16
                        color: Theme.accentText
                    }
                }
                UiText {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: control.title
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
            }

            UiText {
                Layout.fillWidth: true
                text: control.explanation
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                lineHeight: 1.15
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 2
                implicitHeight: 1
                color: Theme.divider
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 4

                UiText {
                    text: i18nc("@label default value of a setting", "Default")
                    color: Theme.textTertiary
                    font.pointSize: Theme.fontSmall
                }
                UiText {
                    Layout.fillWidth: true
                    text: control.defaultText
                    font.pointSize: Theme.fontSmall
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                UiText {
                    visible: control.rangeText.length > 0
                    text: i18nc("@label allowed values of a setting", "Range")
                    color: Theme.textTertiary
                    font.pointSize: Theme.fontSmall
                }
                UiText {
                    Layout.fillWidth: true
                    visible: control.rangeText.length > 0
                    text: control.rangeText
                    font.pointSize: Theme.fontSmall
                    font.weight: Font.Medium
                }
            }

            UiButton {
                Layout.topMargin: 2
                visible: !control.isDefault
                compact: true
                variant: "secondary"
                iconName: "reset"
                text: i18nc("@action:button", "Reset to Default")
                onClicked: {
                    control.resetRequested();
                    popover.close();
                }
            }
        }
    }
}
