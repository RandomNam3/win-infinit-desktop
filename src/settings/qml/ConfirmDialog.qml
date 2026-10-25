// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Modal confirmation for destructive actions. Cancel has the initial focus.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Basic.Popup {
    id: dialog

    property string title
    property string message
    property string confirmText
    property var confirmCallback: null

    function ask(title, message, confirmText, callback) {
        dialog.title = title;
        dialog.message = message;
        dialog.confirmText = confirmText;
        dialog.confirmCallback = callback;
        open();
    }

    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, (parent ? parent.width : 440) - 48)
    modal: true
    focus: true
    padding: 24
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside

    onOpened: cancelButton.forceActiveFocus(Qt.TabFocusReason)

    Basic.Overlay.modal: Rectangle {
        color: Theme.scrim

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationNormal }
        }
    }

    background: Rectangle {
        radius: Theme.radiusXLarge
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.borderStrong

        Shadow {
            radius: parent.radius
            elevation: 2
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        Accessible.role: Accessible.Dialog
        Accessible.name: dialog.title

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: 20
                color: Theme.alpha(Theme.danger, 0.12)

                Icon {
                    anchors.centerIn: parent
                    name: "warning"
                    size: 20
                    color: Theme.danger
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                UiText {
                    Layout.fillWidth: true
                    text: dialog.title
                    font.pointSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: dialog.message
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    lineHeight: 1.12
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 8

            Item {
                Layout.fillWidth: true
            }
            UiButton {
                id: cancelButton
                text: i18nc("@action:button", "Cancel")
                onClicked: dialog.close()
            }
            UiButton {
                variant: "danger"
                text: dialog.confirmText
                onClicked: {
                    const callback = dialog.confirmCallback;
                    dialog.close();
                    if (callback) {
                        callback();
                    }
                }
            }
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
            NumberAnimation { property: "scale"; from: 1; to: 0.97; duration: Theme.durationFast }
        }
    }
}
