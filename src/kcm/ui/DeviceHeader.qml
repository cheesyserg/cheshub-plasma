// SPDX-License-Identifier: GPL-3.0-or-later
// Connection, battery and firmware line shared by both device pages, plus the
// Read from Device / Factory Reset actions.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: header

    required property var device

    spacing: Kirigami.Units.largeSpacing

    Kirigami.Icon {
        source: header.device.kind === "mouse" ? "input-mouse" : "input-keyboard"
        implicitWidth: Kirigami.Units.iconSizes.large
        implicitHeight: implicitWidth
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        Kirigami.Heading {
            level: 3
            text: header.device.name
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.8
            text: {
                const parts = [header.device.link];
                if (header.device.battery >= 0) {
                    parts.push(header.device.charging
                        ? i18nc("@info battery level", "Battery %1% (charging)", header.device.battery)
                        : i18nc("@info battery level", "Battery %1%", header.device.battery));
                }
                if (header.device.firmware !== "")
                    parts.push(i18nc("@info", "Firmware %1", header.device.firmware));
                if (header.device.status !== "")
                    parts.push(header.device.status);
                return parts.join(" · ");
            }
        }
    }

    QQC2.BusyIndicator {
        running: header.device.busy
        visible: running
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
    }

    QQC2.Button {
        icon.name: "view-refresh"
        text: i18nc("@action:button", "Read from Device")
        enabled: !header.device.busy
        onClicked: header.device.sync()
    }

    QQC2.Button {
        icon.name: "edit-reset"
        text: i18nc("@action:button", "Factory Reset…")
        enabled: header.device.ready && !header.device.busy
        onClicked: confirm.open()
    }

    Kirigami.PromptDialog {
        id: confirm
        title: i18nc("@title:window", "Restore Factory Settings?")
        subtitle: header.device.kind === "mouse"
            ? i18n("DPI stages, report rate, button bindings, macros and power settings on the mouse go back to the vendor defaults.")
            : i18n("Lighting, key assignments, macros and per-key colours on the keyboard go back to the vendor defaults.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: header.device.factoryReset()
    }
}
