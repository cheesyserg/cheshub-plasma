// SPDX-License-Identifier: GPL-3.0-or-later
// iLovbee B75: line-art keyboard (click a key to remap it), lighting effects
// with per-effect brightness, speed and palette, debounce, sleep and layout.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: page

    required property var device
    readonly property var effectInfo: device.effects.find(e => e.hw === device.effect) || ({})
    readonly property var remapped: device.keys.filter(k => k.type !== "factory")
    // The page never takes its width from its content (that fed back into the
    // layout and froze System Settings); the column is centred and capped so
    // it stays readable in a maximized window.
    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 56)
        spacing: Kirigami.Units.largeSpacing * 2

        DeviceHeader {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.largeSpacing
            device: page.device
        }

        KeyboardArt {
            Layout.fillWidth: true
            Layout.preferredHeight: width * 399 / 879
            Layout.alignment: Qt.AlignHCenter
            device: page.device
            onKeyClicked: key => keyDialog.openFor(key)
        }

        // Remapped keys, with a quick way back to the default.
        Flow {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing
            visible: page.remapped.length > 0

            QQC2.Label {
                height: resetAll.height
                verticalAlignment: Text.AlignVCenter
                rightPadding: Kirigami.Units.smallSpacing
                text: i18nc("@label", "Remapped:")
            }

            Repeater {
                model: page.remapped
                delegate: Kirigami.Chip {
                    required property var modelData
                    text: i18nc("key → assignment", "%1 → %2", modelData.label, modelData.text)
                    closable: true
                    checkable: false
                    onClicked: keyDialog.openFor(modelData)
                    onRemoved: page.device.resetKey(modelData.id)
                }
            }

            QQC2.Button {
                id: resetAll
                flat: true
                icon.name: "edit-undo"
                text: i18nc("@action:button", "Reset All Keys")
                onClicked: page.device.resetAllKeys()
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
            enabled: page.device.ready && !page.device.busy

            // ---------------------------------------------------------- lighting
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Lighting")
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "Effect:")
                model: page.device.effects
                textRole: "name"
                valueRole: "hw"
                currentIndex: page.device.effects.findIndex(e => e.hw === page.device.effect)
                onActivated: index => page.device.effect = page.device.effects[index].hw
            }

            QQC2.Slider {
                Kirigami.FormData.label: i18nc("@label:slider", "Brightness:")
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                enabled: page.effectInfo.brightness === true
                from: 0
                to: 4
                stepSize: 1
                snapMode: QQC2.Slider.SnapAlways
                value: page.device.brightness
                onMoved: page.device.brightness = value
            }

            QQC2.Slider {
                Kirigami.FormData.label: i18nc("@label:slider", "Speed:")
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                enabled: page.effectInfo.speed === true
                from: 0
                to: 4
                stepSize: 1
                snapMode: QQC2.Slider.SnapAlways
                value: page.device.speed
                onMoved: page.device.speed = value
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18nc("@label:checkbox", "Colour:")
                visible: page.effectInfo.color === true
                text: i18nc("@option:check", "Colourful (cycle through all colours)")
                checked: page.device.colorful
                onToggled: page.device.colorful = checked
            }

            RowLayout {
                Kirigami.FormData.label: i18nc("@label", "Palette:")
                visible: page.effectInfo.color === true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: 7
                    delegate: ColumnLayout {
                        id: slot
                        required property int index
                        spacing: Kirigami.Units.smallSpacing

                        ColorSwatch {
                            color: page.device.palette[slot.index]
                            selected: !page.device.colorful && page.device.colorSlot === slot.index
                            dialogTitle: i18nc("@title:window", "Palette Colour %1", slot.index + 1)
                            onPicked: c => {
                                page.device.setPaletteColor(slot.index, c);
                                page.device.colorSlot = slot.index;
                            }
                        }
                        QQC2.RadioButton {
                            Layout.alignment: Qt.AlignHCenter
                            checked: !page.device.colorful && page.device.colorSlot === slot.index
                            onToggled: page.device.colorSlot = slot.index
                        }
                    }
                }
            }

            QQC2.Label {
                visible: page.device.effect === 19
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                opacity: 0.8
                text: i18n("Per-key colours for the Custom effect can be painted in the ChesHub web studio; this page keeps the colours already on the keyboard.")
            }

            // ---------------------------------------------------------- keyboard
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Keyboard")
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox physical layout", "Top-right key:")
                model: [
                    { value: "knob", text: i18nc("@item:inlistbox", "Rotary knob (retail B75)") },
                    { value: "end", text: i18nc("@item:inlistbox", "End key (no knob)") }
                ]
                textRole: "text"
                valueRole: "value"
                currentIndex: page.device.layout === "end" ? 1 : 0
                onActivated: index => page.device.layout = model[index].value
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox key debounce", "Debounce:")
                model: page.device.debounceOptions
                currentIndex: page.device.debounce
                onActivated: index => page.device.debounce = index
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "Sleep after:")
                model: page.device.sleepOptions
                currentIndex: page.device.sleep
                onActivated: index => page.device.sleep = index
            }

            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                opacity: 0.8
                text: i18n("Click a key on the drawing to remap it on the base layer; the Fn layer keeps its factory functions. The keyboard cannot report its mapping, so assignments made here are remembered on this computer.")
            }
        }
    }

    KeyDialog {
        id: keyDialog
        device: page.device
    }
}
