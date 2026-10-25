// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dialog to pick an installed application (desktop file), or to enter a
// window class that has no desktop file.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import InfinitDesktop.Settings

Basic.Popup {
    id: picker

    property string title: i18nc("@title:window", "Add Application")
    // Entries already in the list, shown as added.
    property var existing: []

    signal picked(string id)

    function choose(id) {
        const value = String(id).trim();
        if (value.length === 0 || existing.indexOf(value) >= 0) {
            return;
        }
        picked(value);
        close();
    }

    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(500, (parent ? parent.width : 500) - 48)
    height: Math.min(620, (parent ? parent.height : 620) - 64)
    modal: true
    focus: true
    padding: 0
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside

    onOpened: {
        searchField.text = "";
        manualField.text = "";
        list.currentIndex = -1;
        list.positionViewAtBeginning();
        searchField.forceActiveFocus(Qt.PopupFocusReason);
    }

    Basic.Overlay.modal: Rectangle {
        color: Theme.scrim
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
        spacing: 0

        Accessible.role: Accessible.Dialog
        Accessible.name: picker.title

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 12
            Layout.topMargin: 16
            Layout.bottomMargin: 10
            spacing: 8

            UiText {
                Layout.fillWidth: true
                text: picker.title
                font.pointSize: Theme.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            IconButton {
                iconName: "close"
                tooltip: i18nc("@action:button", "Close")
                onClicked: picker.close()
            }
        }

        UiTextField {
            id: searchField

            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.bottomMargin: 10
            leftPadding: 34
            placeholderText: i18nc("@info:placeholder", "Search applications…")
            Accessible.name: i18nc("@label", "Search applications")

            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
            onTextChanged: list.currentIndex = text.length > 0 ? 0 : -1
            onAccepted: {
                if (list.currentItem) {
                    list.currentItem.choose();
                }
            }

            Icon {
                x: 11
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 16
                color: Theme.textTertiary
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.divider
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            topMargin: 6
            bottomMargin: 6
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            model: ApplicationFilter {
                id: applications
                query: searchField.text
                highlightColor: Theme.accentText
            }

            Accessible.role: Accessible.List
            Accessible.name: i18nc("@label", "Installed applications")

            delegate: Basic.ItemDelegate {
                id: app

                required property int index
                required property string appId
                required property string name
                required property string genericName
                required property string iconName
                required property bool hasIcon
                required property string highlightedName

                readonly property bool added: picker.existing.indexOf(appId) >= 0

                function choose() {
                    picker.choose(appId);
                }

                width: ListView.view.width
                implicitHeight: 54
                leftPadding: 18
                rightPadding: 18
                hoverEnabled: true
                highlighted: ListView.isCurrentItem
                focusPolicy: Qt.NoFocus

                Accessible.role: Accessible.ListItem
                Accessible.name: added ? i18nc("@info %1 is an application", "%1, already in the list", name) : name
                Accessible.onPressAction: choose()

                onClicked: choose()

                background: Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    radius: Theme.radiusMedium
                    color: app.highlighted || app.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)
                }

                contentItem: RowLayout {
                    spacing: 12

                    AppIcon {
                        iconName: app.iconName
                        hasIcon: app.hasIcon
                        size: 32
                        opacity: app.added ? 0.5 : 1
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        UiText {
                            Layout.fillWidth: true
                            text: app.highlightedName
                            textFormat: Text.StyledText
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            opacity: app.added ? 0.55 : 1
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: [app.genericName, app.appId].filter(s => s.length > 0).join(" · ")
                            color: Theme.textSecondary
                            font.pointSize: Theme.fontSmall
                            elide: Text.ElideRight
                        }
                    }
                    UiText {
                        visible: app.added
                        text: i18nc("@info already in the list", "Added")
                        color: Theme.textTertiary
                        font.pointSize: Theme.fontSmall
                    }
                    Icon {
                        name: app.added ? "check" : "plus"
                        size: 16
                        color: app.added ? Theme.textTertiary : Theme.accentText
                        opacity: app.added || app.hovered || app.highlighted ? 1 : 0
                    }
                }
            }

            UiText {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: i18nc("@info:status", "No application matches “%1”. You can enter its window class below.", searchField.text)
                color: Theme.textSecondary
            }

            Basic.ScrollBar.vertical: UiScrollBar {}
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.divider
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 8

            UiText {
                Layout.fillWidth: true
                text: i18n("Not listed? Enter a window class or desktop file name:")
                color: Theme.textSecondary
                font.pointSize: Theme.fontSmall
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                UiTextField {
                    id: manualField

                    Layout.fillWidth: true
                    placeholderText: i18nc("@info:placeholder", "for example org.kde.konsole")
                    Accessible.name: i18nc("@label", "Window class")
                    onAccepted: picker.choose(text)
                }
                UiButton {
                    text: i18nc("@action:button add an entry to a list", "Add")
                    iconName: "plus"
                    enabled: manualField.text.trim().length > 0 && picker.existing.indexOf(manualField.text.trim()) < 0
                    onClicked: picker.choose(manualField.text)
                }
            }
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
    }
}
