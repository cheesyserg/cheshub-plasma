// SPDX-License-Identifier: GPL-3.0-or-later
// Shared by both modules: kcm.manager only lists the module's device kind
// (mice under Mouse & Touchpad, keyboards under Keyboard).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    readonly property bool keyboards: kcm.manager.kind === "keyboard"
    readonly property var devices: kcm.manager.devices
    property int currentIndex: 0
    readonly property var currentDevice: devices.length > 0 ? devices[Math.min(currentIndex, devices.length - 1)] : null

    implicitWidth: Kirigami.Units.gridUnit * 40
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
    }

    // A single content item whose width never depends on what it contains:
    // the pages size themselves from the view, not the other way round.
    ColumnLayout {
        spacing: 0

        // Only when two devices of this kind are connected.
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.largeSpacing
            visible: root.devices.length > 1
            QQC2.Label {
                text: i18nc("@label:listbox", "Device:")
            }
            QQC2.ComboBox {
                model: root.devices.map(d => i18nc("@item:inlistbox device name and connection", "%1 (%2)", d.name, d.link))
                currentIndex: root.currentIndex
                onActivated: index => root.currentIndex = index
            }
        }

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.gridUnit * 4
            Layout.leftMargin: Kirigami.Units.gridUnit * 2
            Layout.rightMargin: Kirigami.Units.gridUnit * 2
            visible: root.devices.length === 0
            icon.name: root.keyboards ? "input-keyboard" : "input-mouse"
            text: root.keyboards ? i18nc("@info:placeholder", "No supported keyboard found")
                                 : i18nc("@info:placeholder", "No supported mouse found")
            explanation: xi18nc("@info:usagetip %1 is the device name",
                "Plug in %1 or its 2.4 GHz receiver.<nl/><nl/>"
                + "If it is plugged in but not listed, close other configuration tools and make sure the "
                + "<filename>70-cheshub-plasma.rules</filename> udev rule is installed, then re-plug the device.",
                root.keyboards ? i18nc("@info", "an iLovbee B75 keyboard") : i18nc("@info", "a TECKNET TK-GM012 mouse"))
            helpfulAction: Kirigami.Action {
                icon.name: "view-refresh"
                text: i18nc("@action:button", "Search Again")
                onTriggered: kcm.manager.rescan()
            }
        }

        Loader {
            Layout.fillWidth: true
            active: root.currentDevice !== null
            visible: active
            sourceComponent: root.currentDevice && root.currentDevice.kind === "keyboard" ? keyboardPage : mousePage
        }
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
