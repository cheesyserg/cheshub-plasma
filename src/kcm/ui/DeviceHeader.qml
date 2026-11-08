// SPDX-License-Identifier: GPL-3.0-or-later
// Device name, link, battery and firmware, plus Read from Device and
// Factory Reset, as a card that stays compact in a maximized window.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.AbstractCard {
    id: header

    required property var device

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: header.device.kind === "mouse" ? "input-mouse" : "input-keyboard"
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: implicitWidth
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Kirigami.Heading {
                level: 4
                text: header.device.name
            }
            RowLayout {
                spacing: Kirigami.Units.largeSpacing
                QQC2.Label {
                    opacity: 0.8
                    text: header.device.link
                }
                RowLayout {
                    visible: header.device.battery >= 0
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                        source: {
                            const b = header.device.battery;
                            const level = b >= 90 ? "100" : b >= 70 ? "080" : b >= 50 ? "060" : b >= 30 ? "040" : b >= 10 ? "020" : "000";
                            return "battery-" + level + (header.device.charging ? "-charging" : "");
                        }
                    }
                    QQC2.Label {
                        opacity: 0.8
                        text: i18nc("@info battery level", "%1%", header.device.battery)
                    }
                }
                QQC2.Label {
                    visible: header.device.firmware !== ""
                    opacity: 0.8
                    text: i18nc("@info", "Firmware %1", header.device.firmware)
                }
                QQC2.Label {
                    visible: header.device.status !== ""
                    opacity: 0.8
                    font.italic: true
                    text: header.device.status
                }
            }
        }

        QQC2.BusyIndicator {
            running: header.device.busy
            visible: running
            implicitWidth: Kirigami.Units.iconSizes.smallMedium
            implicitHeight: implicitWidth
        }

        QQC2.ToolButton {
            icon.name: "view-refresh"
            text: i18nc("@action:button", "Read from Device")
            display: QQC2.AbstractButton.IconOnly
            enabled: !header.device.busy
            onClicked: header.device.sync()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }

        QQC2.ToolButton {
            icon.name: "edit-reset"
            text: i18nc("@action:button", "Factory Reset…")
            display: QQC2.AbstractButton.IconOnly
            enabled: header.device.ready && !header.device.busy
            onClicked: confirm.open()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }
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
