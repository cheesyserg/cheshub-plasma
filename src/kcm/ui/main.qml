// SPDX-License-Identifier: GPL-3.0-or-later
// ChesHub Devices: the connected supported devices. Each row opens that
// device's page as a sub-page, the way KDE's own modules drill down.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.ScrollViewKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 40
    implicitHeight: Kirigami.Units.gridUnit * 34

    function open(device) {
        kcm.push(device.kind === "keyboard" ? "KeyboardPage.qml" : "MousePage.qml", { device: device });
    }

    header: ColumnLayout {
        spacing: 0
        Repeater {
            model: kcm.manager.notices
            delegate: Kirigami.InlineMessage {
                required property string modelData
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                visible: true
                text: modelData
            }
        }
    }

    view: ListView {
        id: list
        model: kcm.manager.devices
        clip: true

        delegate: QQC2.ItemDelegate {
            id: row
            required property var modelData
            readonly property var device: modelData

            width: ListView.view.width
            padding: Kirigami.Units.largeSpacing * 2
            onClicked: root.open(device)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing * 2

                Kirigami.Icon {
                    source: row.device.kind === "keyboard" ? "input-keyboard" : "input-mouse"
                    implicitWidth: Kirigami.Units.iconSizes.large
                    implicitHeight: implicitWidth
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 4
                        elide: Text.ElideRight
                        text: row.device.name
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        opacity: 0.7
                        text: {
                            const parts = [row.device.kind === "keyboard" ? i18nc("@info device type", "Keyboard")
                                                                          : i18nc("@info device type", "Mouse"),
                                           row.device.link];
                            if (row.device.error !== "")
                                parts.push(i18nc("@info", "Needs attention"));
                            else if (!row.device.ready)
                                parts.push(i18nc("@info", "Connecting…"));
                            return parts.join(" · ");
                        }
                    }
                }

                BatteryLabel {
                    device: row.device
                }

                Kirigami.Icon {
                    source: "go-next-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: implicitWidth
                    opacity: 0.6
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "input-mouse"
            text: i18nc("@info:placeholder", "No supported device connected")
            explanation: xi18nc("@info:usagetip",
                "Plug in a TECKNET TK-GM012 mouse or an iLovbee B75 keyboard, or its 2.4 GHz receiver. It appears here as soon as it is connected.<nl/><nl/>"
                + "If it is plugged in but not listed, close other configuration tools and make sure the "
                + "<filename>70-cheshub-plasma.rules</filename> udev rule is installed, then re-plug the device.")
            helpfulAction: Kirigami.Action {
                icon.name: "view-refresh"
                text: i18nc("@action:button", "Search Again")
                onTriggered: kcm.manager.rescan()
            }
        }
    }
}
