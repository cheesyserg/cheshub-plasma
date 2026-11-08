// SPDX-License-Identifier: GPL-3.0-or-later
// Pick what one B75 key sends: a key with optional modifiers, a media
// function, a mouse button, nothing, or its factory default.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: dialog

    required property var device
    property var key: null
    property int modifierBits: 0

    function openFor(k) {
        key = k;
        modifierBits = 0;
        kind.currentIndex = 1;
        open();
    }

    title: key ? i18nc("@title:window", "Assign Key: %1", key.label) : ""
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 24
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

    onAccepted: {
        const id = key.id;
        switch (kind.currentValue) {
        case "factory":
            device.resetKey(id);
            break;
        case "keyboard":
            device.assignKeyboard(id, modifierBits, device.keyboardUsages[usage.currentIndex].usage);
            break;
        case "media":
            device.assignMedia(id, device.mediaActions[media.currentIndex].code);
            break;
        case "mouse":
            device.assignMouse(id, device.mouseActions[mouse.currentIndex].code);
            break;
        case "disabled":
            device.disableKey(id);
            break;
        }
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: kind
            Kirigami.FormData.label: i18nc("@label:listbox", "Action:")
            model: [
                { value: "factory", text: i18nc("@item:inlistbox", "Default") },
                { value: "keyboard", text: i18nc("@item:inlistbox", "Key or shortcut") },
                { value: "media", text: i18nc("@item:inlistbox", "Media or application") },
                { value: "mouse", text: i18nc("@item:inlistbox", "Mouse button or scroll") },
                { value: "disabled", text: i18nc("@item:inlistbox", "Disabled") }
            ]
            textRole: "text"
            valueRole: "value"
        }

        Flow {
            Kirigami.FormData.label: i18nc("@label", "Modifiers:")
            Layout.maximumWidth: Kirigami.Units.gridUnit * 16
            visible: kind.currentValue === "keyboard"
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: dialog.device.modifiers
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData.name
                    checked: (dialog.modifierBits & modelData.bit) !== 0
                    onToggled: dialog.modifierBits = checked ? (dialog.modifierBits | modelData.bit)
                                                            : (dialog.modifierBits & ~modelData.bit)
                }
            }
        }

        QQC2.ComboBox {
            id: usage
            Kirigami.FormData.label: i18nc("@label:listbox", "Key:")
            visible: kind.currentValue === "keyboard"
            model: dialog.device.keyboardUsages
            textRole: "name"
        }

        QQC2.ComboBox {
            id: media
            Kirigami.FormData.label: i18nc("@label:listbox", "Function:")
            visible: kind.currentValue === "media"
            model: dialog.device.mediaActions
            textRole: "name"
        }

        QQC2.ComboBox {
            id: mouse
            Kirigami.FormData.label: i18nc("@label:listbox", "Mouse:")
            visible: kind.currentValue === "mouse"
            model: dialog.device.mouseActions
            textRole: "name"
        }
    }
}
