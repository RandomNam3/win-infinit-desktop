// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Navigation: brand, search slot, page list and a compact status line.
// Collapses to an icon rail in narrow windows.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

Rectangle {
    id: sidebar

    property bool compact: false
    property string currentPage
    property ListModel pages
    // The search field is reparented into this slot in wide mode.
    property alias searchSlot: searchSlot

    signal navigate(string pageId)
    signal searchRequested()

    color: Theme.sidebar

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.divider
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: sidebar.compact ? 10 : 14
        anchors.rightMargin: sidebar.compact ? 11 : 15
        anchors.topMargin: 16
        anchors.bottomMargin: 14
        spacing: 0

        // Brand
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: sidebar.compact ? 0 : 6
            Layout.bottomMargin: 16
            spacing: 10

            Image {
                Layout.alignment: sidebar.compact ? Qt.AlignHCenter : Qt.AlignLeft
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                source: "qrc:/infinitedesktop/icons/app-icon.svg"
                sourceSize: Qt.size(32, 32)
                smooth: true
                Accessible.ignored: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: !sidebar.compact
                spacing: 0

                UiText {
                    Layout.fillWidth: true
                    text: i18nc("@title short product name", "Infinite Desktop")
                    font.pointSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                UiText {
                    Layout.fillWidth: true
                    text: i18nc("@title subtitle of the product name", "Infinite canvas for Windows")
                    font.pointSize: Theme.fontCaption
                    color: Theme.textTertiary
                    elide: Text.ElideRight
                }
            }
        }

        Item {
            id: searchSlot
            Layout.fillWidth: true
            Layout.preferredHeight: sidebar.compact ? 0 : 38
            Layout.bottomMargin: sidebar.compact ? 0 : 14
            visible: !sidebar.compact
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: sidebar.pages
            spacing: 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true
            activeFocusOnTab: true
            currentIndex: {
                for (let i = 0; i < sidebar.pages.count; ++i) {
                    if (sidebar.pages.get(i).pageId === sidebar.currentPage) {
                        return i;
                    }
                }
                return -1;
            }

            Accessible.role: Accessible.PageTabList
            Accessible.name: i18nc("@label accessible name of the page list", "Pages")

            Keys.onReturnPressed: sidebar.navigate(sidebar.pages.get(currentIndex).pageId)
            Keys.onEnterPressed: sidebar.navigate(sidebar.pages.get(currentIndex).pageId)
            Keys.onSpacePressed: sidebar.navigate(sidebar.pages.get(currentIndex).pageId)
            Keys.onUpPressed: {
                if (currentIndex > 0) {
                    sidebar.navigate(sidebar.pages.get(currentIndex - 1).pageId);
                }
            }
            Keys.onDownPressed: {
                if (currentIndex < count - 1) {
                    sidebar.navigate(sidebar.pages.get(currentIndex + 1).pageId);
                }
            }

            section.property: "section"
            section.delegate: Item {
                required property string section
                width: ListView.view.width
                height: section.length === 0 ? 0 : sidebar.compact ? 13 : 30
                visible: section.length > 0

                UiText {
                    visible: !sidebar.compact
                    x: 10
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    text: parent.section
                    font.pointSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                    color: Theme.textTertiary
                }
                Rectangle {
                    visible: sidebar.compact
                    anchors.centerIn: parent
                    width: 24
                    height: 1
                    color: Theme.divider
                }
            }

            delegate: NavItem {
                required property string pageId
                required property string pageTitle
                required property string pageIcon
                required property int index

                width: ListView.view.width
                text: pageTitle
                iconName: pageIcon
                selected: pageId === sidebar.currentPage
                compact: sidebar.compact
                keyboardCurrent: list.activeFocus && ListView.isCurrentItem
                onClicked: sidebar.navigate(pageId)
            }
        }

        // Status line
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.preferredHeight: 40
            radius: Theme.radiusMedium
            color: statusArea.containsMouse ? Theme.hover : Theme.alpha(Theme.hover, 0)

            StatusDot {
                id: dot
                x: sidebar.compact ? (parent.width - width) / 2 : 12
                anchors.verticalCenter: parent.verticalCenter
                status: Effect.state
            }
            UiText {
                anchors.left: dot.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: !sidebar.compact
                text: StatusTexts.title(Effect.state)
                font.pointSize: Theme.fontSmall
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
            MouseArea {
                id: statusArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: sidebar.navigate("overview")
            }
            UiToolTip {
                text: StatusTexts.title(Effect.state)
                visible: sidebar.compact && statusArea.containsMouse
            }
        }
    }
}
