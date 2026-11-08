// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    readonly property var devices: kcm.manager.devices
    property int currentIndex: 0
    readonly property var currentDevice: devices.length > 0 ? devices[Math.min(currentIndex, devices.length - 1)] : null

    implicitWidth: Kirigami.Units.gridUnit * 38
    implicitHeight: Kirigami.Units.gridUnit * 34

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

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            type: Kirigami.MessageType.Error
            visible: root.currentDevice !== null && root.currentDevice.error !== ""
            text: root.currentDevice ? root.currentDevice.error : ""
            actions: [
                Kirigami.Action {
                    icon.name: "view-refresh"
                    text: i18nc("@action:button", "Try Again")
                    onTriggered: root.currentDevice.sync()
                }
            ]
        }

        QQC2.TabBar {
            id: tabs
            Layout.fillWidth: true
            visible: root.devices.length > 1
            currentIndex: root.currentIndex
            onCurrentIndexChanged: root.currentIndex = currentIndex

            Repeater {
                model: root.devices
                delegate: QQC2.TabButton {
                    required property var modelData
                    text: i18nc("@title:tab device name and connection", "%1 (%2)", modelData.name, modelData.link)
                    icon.name: modelData.kind === "mouse" ? "input-mouse" : "input-keyboard"
                }
            }
        }
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: root.devices.length === 0
        icon.name: "input-mouse"
        text: i18nc("@info:placeholder", "No supported device found")
        explanation: xi18nc("@info:usagetip",
            "Plug in a TECKNET TK-GM012 mouse or an iLovbee B75 keyboard (or its 2.4 GHz receiver).<nl/><nl/>"
            + "If it is plugged in but not listed, close other configuration tools and make sure the "
            + "<filename>70-cheshub-plasma.rules</filename> udev rule is installed, then re-plug the device.")
        helpfulAction: Kirigami.Action {
            icon.name: "view-refresh"
            text: i18nc("@action:button", "Search Again")
            onTriggered: kcm.manager.rescan()
        }
    }

    Loader {
        id: page
        width: parent.width
        active: root.currentDevice !== null
        sourceComponent: root.currentDevice && root.currentDevice.kind === "keyboard" ? keyboardPage : mousePage
    }

    Component {
        id: mousePage
        MousePage {
            device: root.currentDevice
        }
    }

    Component {
        id: keyboardPage
        KeyboardPage {
            device: root.currentDevice
        }
    }
}
