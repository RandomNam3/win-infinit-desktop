// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One setting: title with "i" button, short hint and the matching control.
// Used as delegate for SettingsFilter models.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

Item {
    id: row

    required property int index
    required property string key
    required property string label
    required property string info
    required property string hint
    required property string type
    required property var value
    required property var defaultValue
    required property bool isDefault
    required property int min
    required property int max
    required property int step
    required property string unit
    required property var choices
    required property bool dependencyMet
    required property string dependsOnLabel
    required property string valueText
    required property string defaultText
    required property string rangeText
    required property string highlightedLabel
    required property string highlightedHint

    property bool last: false
    property bool searchMode: false

    readonly property real horizontalPadding: 18
    readonly property real innerWidth: width - 2 * horizontalPadding
    readonly property real segmentLimit: Math.min(380, innerWidth * 0.5)
    readonly property bool stacked: type === "stringlist" || (type !== "bool" && innerWidth - controlLoader.implicitWidth - 20 < 280)
    property bool awaitingApply: false

    function openInfo() {
        infoButton.openPopover();
    }

    function apply(newValue) {
        awaitingApply = true;
        SettingsStore.setValue(key, newValue);
    }

    implicitHeight: content.implicitHeight + 28
    Accessible.role: Accessible.Grouping
    Accessible.name: label

    Connections {
        target: SettingsStore
        function onApplied() {
            if (row.awaitingApply) {
                row.awaitingApply = false;
                appliedMark.flash();
            }
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: Theme.radiusMedium
        color: hoverHandler.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }

    GridLayout {
        id: content

        x: row.horizontalPadding
        width: row.innerWidth
        anchors.verticalCenter: parent.verticalCenter
        columns: row.stacked ? 1 : 2
        columnSpacing: 20
        rowSpacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 3

            Item {
                id: titleLine

                Layout.fillWidth: true
                implicitHeight: Math.max(titleText.height, infoButton.height)

                UiText {
                    id: titleText

                    width: Math.min(implicitWidth, titleLine.width - infoButton.width - 6)
                    y: Math.max(0, (infoButton.height - titleMetrics.height) / 2)
                    text: row.searchMode ? row.highlightedLabel : row.label
                    textFormat: row.searchMode ? Text.StyledText : Text.PlainText
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                    opacity: row.dependencyMet ? 1 : 0.6
                }
                FontMetrics {
                    id: titleMetrics
                    font: titleText.font
                }
                InfoButton {
                    id: infoButton

                    x: titleText.contentWidth + 6
                    title: row.label
                    explanation: row.info
                    defaultText: row.defaultText
                    rangeText: row.rangeText
                    isDefault: row.isDefault
                    onResetRequested: SettingsStore.resetToDefault(row.key)
                }
            }

            UiText {
                Layout.fillWidth: true
                text: row.searchMode ? row.highlightedHint : row.hint
                textFormat: row.searchMode ? Text.StyledText : Text.PlainText
                color: Theme.textSecondary
                font.pointSize: Theme.fontSmall
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                visible: text.length > 0
                opacity: row.dependencyMet ? 1 : 0.6
            }

            RowLayout {
                Layout.topMargin: 3
                visible: !row.dependencyMet
                spacing: 5

                Icon {
                    name: "link"
                    size: 13
                    color: Theme.textTertiary
                }
                UiText {
                    Layout.fillWidth: true
                    text: i18nc("@info %1 is the name of another setting", "Requires “%1”", row.dependsOnLabel)
                    color: Theme.textTertiary
                    font.pointSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
            }
        }

        RowLayout {
            Layout.alignment: row.stacked ? Qt.AlignLeft | Qt.AlignVCenter : Qt.AlignRight | Qt.AlignVCenter
            Layout.fillWidth: row.stacked && (row.type === "stringlist" || row.type === "int")
            layoutDirection: row.stacked ? Qt.RightToLeft : Qt.LeftToRight
            spacing: 6

            Icon {
                id: appliedMark

                function flash() {
                    flashAnimation.restart();
                }

                name: "check"
                size: 16
                color: Theme.success
                opacity: 0
                visible: row.type !== "stringlist"

                SequentialAnimation {
                    id: flashAnimation
                    NumberAnimation { target: appliedMark; property: "opacity"; to: 1; duration: Theme.durationFast }
                    PauseAnimation { duration: 700 }
                    NumberAnimation { target: appliedMark; property: "opacity"; to: 0; duration: Theme.durationSlow }
                }
            }

            IconButton {
                id: resetButton

                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                iconName: "reset"
                iconSize: 15
                tooltip: i18nc("@action:button %1 is the default value", "Reset to default (%1)", row.defaultText)
                opacity: row.isDefault ? 0 : 1
                visible: opacity > 0
                onClicked: SettingsStore.resetToDefault(row.key)

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationNormal }
                }
            }

            Loader {
                id: controlLoader

                Layout.fillWidth: row.stacked && (row.type === "stringlist" || row.type === "int")
                Layout.alignment: Qt.AlignVCenter
                enabled: row.dependencyMet
                sourceComponent: {
                    switch (row.type) {
                    case "bool":
                        return boolControl;
                    case "int":
                        return intControl;
                    case "enum":
                        return enumControl;
                    case "color":
                        return colorControl;
                    case "stringlist":
                        return listControl;
                    }
                    return null;
                }
            }
        }
    }

    Rectangle {
        visible: !row.last
        x: row.horizontalPadding
        width: row.innerWidth
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.divider
    }

    Component {
        id: boolControl

        ToggleSwitch {
            checked: row.value
            Accessible.name: row.label
            onToggled: {
                row.apply(checked);
                checked = Qt.binding(() => row.value);
            }
        }
    }

    Component {
        id: intControl

        RowLayout {
            spacing: 10

            SliderControl {
                id: slider

                Layout.fillWidth: row.stacked
                Layout.preferredWidth: 184
                from: row.min
                to: row.max
                stepSize: row.step
                value: row.value
                defaultValue: row.defaultValue
                valueText: SettingsStore.formatValue(row.key, value)
                Accessible.name: row.label
                onMoved: {
                    row.apply(value);
                    value = Qt.binding(() => row.value);
                }
            }
            Stepper {
                from: row.min
                to: row.max
                stepSize: row.step
                value: row.value
                displayText: row.valueText
                accessibleName: row.label
                onValueModified: newValue => row.apply(newValue)
            }
        }
    }

    Component {
        id: enumControl

        Item {
            id: enumItem

            readonly property bool useSegments: row.choices.length <= 4 && segments.implicitWidth <= row.segmentLimit

            implicitWidth: useSegments ? segments.implicitWidth : dropdown.implicitWidth
            implicitHeight: Theme.controlHeight

            SegmentedControl {
                id: segments

                visible: enumItem.useSegments
                model: row.choices
                currentIndex: row.value
                accessibleName: row.label
                onActivated: index => row.apply(index)
            }
            Dropdown {
                id: dropdown

                visible: !enumItem.useSegments
                width: implicitWidth
                maximumWidth: Math.max(200, row.innerWidth)
                model: row.choices
                currentIndex: row.value
                accessibleName: row.label
                onActivated: index => {
                    row.apply(index);
                    currentIndex = Qt.binding(() => row.value);
                }
            }
        }
    }

    Component {
        id: colorControl

        ColorControl {
            value: row.value
            accessibleName: row.label
            onPicked: color => row.apply(color)
        }
    }

    Component {
        id: listControl

        AppListEditor {
            items: row.value
            accessibleName: row.label
            onEdited: items => row.apply(items)
        }
    }
}
