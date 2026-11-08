// SPDX-License-Identifier: GPL-3.0-or-later
// Shell for a device's sub-page: page title, Read from Device / Factory
// Reset as page actions, a quiet status line, errors as an inline message,
// and the device content (`body`) centred at a readable width.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: devicePage

    // Null once the device is unplugged.
    property var device: null
    property Component body
    property real maximumContentWidth: Kirigami.Units.gridUnit * 50

    title: device ? device.name : i18nc("@title", "Device Disconnected")

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18nc("@action:button", "Read from Device")
            enabled: devicePage.device !== null && !devicePage.device.busy
            onTriggered: devicePage.device.sync()
        },
        Kirigami.Action {
            icon.name: "edit-reset"
            text: i18nc("@action:button", "Factory Reset…")
            enabled: devicePage.device !== null && devicePage.device.ready && !devicePage.device.busy
            onTriggered: confirm.open()
        }
    ]

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        visible: devicePage.device !== null && devicePage.device.error !== ""
        text: devicePage.device ? devicePage.device.error : ""
        actions: [
            Kirigami.Action {
                icon.name: "view-refresh"
                text: i18nc("@action:button", "Try Again")
                onTriggered: devicePage.device.sync()
            }
        ]
    }

    // The page never takes its width from its content: the column is
    // centred and capped, so width only flows downward.
    Item {
        implicitWidth: Kirigami.Units.gridUnit * 30
        implicitHeight: devicePage.device ? column.implicitHeight : gone.implicitHeight

        ColumnLayout {
            id: column
            visible: devicePage.device !== null
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, devicePage.maximumContentWidth)
            spacing: Kirigami.Units.largeSpacing * 2

            // Connection, battery, firmware and what the device is doing.
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.largeSpacing * 2
                opacity: 0.75

                QQC2.Label {
                    text: devicePage.device ? devicePage.device.link : ""
                }
                BatteryLabel {
                    device: devicePage.device
                }
                QQC2.Label {
                    visible: devicePage.device !== null && devicePage.device.firmware !== ""
                    text: devicePage.device ? i18nc("@info", "Firmware %1", devicePage.device.firmware) : ""
                }
                RowLayout {
                    visible: devicePage.device !== null && devicePage.device.busy
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.BusyIndicator {
                        running: parent.visible
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                    }
                    QQC2.Label {
                        text: devicePage.device ? devicePage.device.status : ""
                    }
                }
            }

            Loader {
                Layout.fillWidth: true
                active: devicePage.device !== null
                sourceComponent: devicePage.body
            }
        }

        Kirigami.PlaceholderMessage {
            id: gone
            visible: devicePage.device === null
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            icon.name: "network-disconnect"
            text: i18nc("@info:placeholder", "This device was disconnected")
            explanation: i18nc("@info:usagetip", "Plug it back in and open it again from the device list.")
        }
    }

    Kirigami.PromptDialog {
        id: confirm
        title: i18nc("@title:window", "Restore Factory Settings?")
        subtitle: devicePage.device && devicePage.device.kind === "keyboard"
            ? i18n("Lighting, key assignments, macros and per-key colours on the keyboard go back to the vendor defaults.")
            : i18n("DPI stages, report rate, button bindings, macros and power settings on the mouse go back to the vendor defaults.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: devicePage.device.factoryReset()
    }
}
