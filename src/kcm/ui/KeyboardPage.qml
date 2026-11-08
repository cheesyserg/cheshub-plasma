// SPDX-License-Identifier: GPL-3.0-or-later
// iLovbee B75: lighting effects (per-effect brightness, speed and palette),
// debounce, sleep, layout and base-layer key remapping.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: page

    required property var device
    readonly property var effectInfo: device.effects.find(e => e.hw === device.effect) || ({})

    spacing: Kirigami.Units.largeSpacing

    DeviceHeader {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        device: page.device
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true
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

        // ---------------------------------------------------------- settings
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Keyboard")
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

        // --------------------------------------------------------------- keys
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Key Mapping")
        }

        RowLayout {
            Kirigami.FormData.label: i18nc("@label:textbox", "Find key:")
            Kirigami.SearchField {
                id: search
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
            }
            QQC2.Button {
                icon.name: "edit-undo"
                text: i18nc("@action:button", "Reset All Keys")
                onClicked: page.device.resetAllKeys()
            }
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        opacity: 0.8
        text: i18n("Changes the base layer. The keyboard cannot report its key mapping, so assignments made here are remembered on this computer. The Fn layer keeps its factory functions.")
    }

    Kirigami.CardsLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        enabled: page.device.ready && !page.device.busy
        maximumColumnWidth: Kirigami.Units.gridUnit * 14
        minimumColumnWidth: Kirigami.Units.gridUnit * 10

        Repeater {
            model: page.device.keys.filter(k => search.text === ""
                || k.label.toLowerCase().includes(search.text.toLowerCase())
                || k.text.toLowerCase().includes(search.text.toLowerCase()))
            delegate: Kirigami.AbstractCard {
                id: card
                required property var modelData

                contentItem: RowLayout {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        QQC2.Label {
                            text: card.modelData.label
                            font.bold: true
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            opacity: card.modelData.type === "factory" ? 0.6 : 1
                            text: card.modelData.text
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: "document-edit"
                        display: QQC2.AbstractButton.IconOnly
                        text: i18nc("@action:button", "Change %1", card.modelData.label)
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                        onClicked: keyDialog.openFor(card.modelData)
                    }
                    QQC2.ToolButton {
                        icon.name: "edit-undo"
                        display: QQC2.AbstractButton.IconOnly
                        visible: card.modelData.type !== "factory"
                        text: i18nc("@action:button", "Restore default")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                        onClicked: page.device.resetKey(card.modelData.id)
                    }
                }
            }
        }
    }

    KeyDialog {
        id: keyDialog
        device: page.device
    }
}
