// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The window positions the effect remembers, with an expandable list and
// "forget all".
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

ColumnLayout {
    id: root

    property bool expanded: false

    signal forgetRequested()

    readonly property int applicationCount: RememberedWindows.applications.length

    Layout.fillWidth: true
    spacing: 8

    Component.onCompleted: RememberedWindows.refresh()

    // Ask again when the effect gets loaded or unloaded.
    property bool effectLoaded: Effect.loaded
    onEffectLoadedChanged: RememberedWindows.refresh()

    SectionLabel {
        text: i18nc("@title", "Remembered windows")
    }

    Card {
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 16

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.radiusMedium
                color: Theme.accentSofter

                Icon {
                    anchors.centerIn: parent
                    name: "layers"
                    size: 18
                    color: Theme.accentText
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UiText {
                    Layout.fillWidth: true
                    text: !RememberedWindows.available ? i18nc("@info:status", "Not available right now")
                          : root.applicationCount === 0 ? i18nc("@info:status", "No remembered windows yet")
                          : i18ncp("@info:status", "Positions of %1 application remembered", "Positions of %1 applications remembered", root.applicationCount)
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: !RememberedWindows.available ? i18n("The list is shown while the canvas service is running.")
                          : i18n("Reopened applications appear where their windows were closed, on the same screen and canvas position.")
                    color: Theme.textSecondary
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }

            UiButton {
                visible: RememberedWindows.available && root.applicationCount > 0
                variant: "ghost"
                compact: true
                iconName: root.expanded ? "chevron-down" : "chevron-right"
                text: root.expanded ? i18nc("@action:button collapse a list", "Hide") : i18nc("@action:button expand a list", "Show")
                Accessible.name: root.expanded ? i18nc("@action:button", "Hide remembered windows") : i18nc("@action:button", "Show remembered windows")
                onClicked: root.expanded = !root.expanded
            }
            UiButton {
                visible: RememberedWindows.available && root.applicationCount > 0
                variant: "dangerSoft"
                compact: true
                text: i18nc("@action:button", "Forget All…")
                onClicked: root.forgetRequested()
            }
        }

        // Expandable list of applications
        Item {
            Layout.fillWidth: true
            implicitHeight: root.expanded ? list.implicitHeight : 0
            clip: true
            visible: implicitHeight > 0

            Behavior on implicitHeight {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                id: list

                width: parent.width
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: 1
                    color: Theme.divider
                }

                Repeater {
                    model: root.expanded ? RememberedWindows.applications : []

                    Item {
                        id: app

                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        implicitHeight: appRow.implicitHeight + 20

                        RowLayout {
                            id: appRow

                            x: 18
                            width: parent.width - 26
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 14

                            AppIcon {
                                Layout.alignment: Qt.AlignTop
                                iconName: app.modelData.iconName
                                hasIcon: app.modelData.hasIcon
                                size: 32
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                UiText {
                                    Layout.fillWidth: true
                                    text: app.modelData.name
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Repeater {
                                    model: app.modelData.windows

                                    RowLayout {
                                        required property var modelData

                                        Layout.fillWidth: true
                                        spacing: 6

                                        UiText {
                                            Layout.fillWidth: true
                                            text: [modelData.screen, modelData.size].filter(s => s.length > 0).join(" · ")
                                            color: Theme.textSecondary
                                            font.pointSize: Theme.fontSmall
                                            elide: Text.ElideRight
                                        }
                                        Repeater {
                                            model: modelData.flags

                                            Rectangle {
                                                required property string modelData
                                                Layout.preferredWidth: flagText.implicitWidth + 12
                                                Layout.preferredHeight: 18
                                                radius: 9
                                                color: Theme.control

                                                UiText {
                                                    id: flagText
                                                    anchors.centerIn: parent
                                                    text: parent.modelData
                                                    font.pointSize: Theme.fontCaption
                                                    color: Theme.textSecondary
                                                }
                                            }
                                        }
                                        UiText {
                                            visible: modelData.time.length > 0
                                            text: modelData.time
                                            color: Theme.textTertiary
                                            font.pointSize: Theme.fontCaption
                                        }
                                    }
                                }
                            }

                            IconButton {
                                Layout.alignment: Qt.AlignVCenter
                                iconName: "trash"
                                iconSize: 15
                                tooltip: i18nc("@action:button %1 is an application", "Forget the position of “%1”", app.modelData.name)
                                onClicked: RememberedWindows.forgetApplication(app.modelData.appKey)
                            }
                        }

                        Rectangle {
                            visible: app.index < root.applicationCount - 1
                            anchors.bottom: parent.bottom
                            x: 64
                            width: parent.width - 82
                            height: 1
                            color: Theme.divider
                        }
                    }
                }
            }
        }
    }
}
