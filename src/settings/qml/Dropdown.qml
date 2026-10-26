// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Drop-down list for enum settings with many or long choices.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Basic.ComboBox {
    id: control

    property real maximumWidth: 320
    property string accessibleName

    property real widestText: 0

    function measureWidest() {
        let widest = 0;
        const items = model || [];
        for (let i = 0; i < items.length; ++i) {
            metrics.text = String(items[i]);
            widest = Math.max(widest, metrics.advanceWidth);
        }
        widestText = widest;
    }

    onModelChanged: measureWidest()
    onFontChanged: measureWidest()
    Component.onCompleted: measureWidest()

    implicitWidth: Math.min(maximumWidth, Math.max(160, widestText + leftPadding + rightPadding + 2))
    implicitHeight: Theme.controlHeight
    leftPadding: 12
    rightPadding: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.pointSize: Theme.fontBody
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName

    TextMetrics {
        id: metrics
        font: control.font
    }

    contentItem: UiText {
        text: control.displayText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: control.width - width - 12
        y: (control.height - height) / 2
        name: "chevron-down"
        size: 16
        color: Theme.textSecondary
        rotation: control.popup.visible ? 180 : 0

        Behavior on rotation {
            NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMedium
        color: control.pressed ? Theme.controlPressed : control.hovered ? Theme.controlHover : Theme.control

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus
        }
    }

    delegate: Basic.ItemDelegate {
        id: entry

        required property int index
        required property var modelData

        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: 34
        leftPadding: 10
        rightPadding: 10
        highlighted: control.highlightedIndex === index
        hoverEnabled: true

        Accessible.role: Accessible.ListItem
        Accessible.name: String(modelData)

        contentItem: RowLayout {
            spacing: 10

            UiText {
                Layout.fillWidth: true
                text: String(entry.modelData)
                elide: Text.ElideRight
                font.weight: control.currentIndex === entry.index ? Font.Medium : Font.Normal
            }
            Icon {
                name: "check"
                size: 16
                color: Theme.accentText
                opacity: control.currentIndex === entry.index ? 1 : 0
            }
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: entry.highlighted || entry.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)
        }
    }

    popup: Basic.Popup {
        y: control.height + 6
        width: Math.max(control.width, control.widestText + 66)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 360)
        padding: 5
        margins: 12

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds
        }

        background: Rectangle {
            radius: Theme.radiusLarge
            color: Theme.surfaceRaised
            border.width: 1
            border.color: Theme.borderStrong

            Shadow {
                radius: parent.radius
                elevation: 2
            }
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast }
                NumberAnimation { property: "y"; from: control.height; to: control.height + 6; duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
        }
    }
}
