// SPDX-License-Identifier: GPL-3.0-or-later
// Battery icon and percentage; hidden when the device does not report one.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: battery

    required property var device

    visible: device !== null && device.battery >= 0
    spacing: Kirigami.Units.smallSpacing

    Kirigami.Icon {
        implicitWidth: Kirigami.Units.iconSizes.small
        implicitHeight: implicitWidth
        source: {
            if (!battery.device)
                return "";
            const b = battery.device.battery;
            const level = b >= 90 ? "100" : b >= 70 ? "080" : b >= 50 ? "060" : b >= 30 ? "040" : b >= 10 ? "020" : "000";
            return "battery-" + level + (battery.device.charging ? "-charging" : "");
        }
    }
    QQC2.Label {
        text: battery.device ? i18nc("@info battery level", "%1%", battery.device.battery) : ""
    }
}
