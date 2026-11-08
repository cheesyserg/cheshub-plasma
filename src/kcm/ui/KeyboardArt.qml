// SPDX-License-Identifier: GPL-3.0-or-later
// Line-art iLovbee B75, ported from the web studio (case, bezel, key well,
// badge and keycap grid in an 879 x 399 drawing), in Kirigami theme colours.
// Keys are clickable; remapped keys are marked. The caps glow with an
// approximation of the selected lighting effect.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: art

    required property var device
    signal keyClicked(var key)

    readonly property real k: width / 879
    readonly property color line: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.62)
    readonly property color soft: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.26)

    // ---------------------------------------------------- lighting preview
    readonly property var effect: device.effects.find(e => e.hw === device.effect) || ({})
    readonly property bool lit: device.effect !== 0 && device.effect !== 19
    readonly property bool rainbow: lit && (effect.color !== true || device.colorful)
    readonly property real level: effect.brightness === true ? device.brightness / 4 : 1
    property real phase: 0

    NumberAnimation on phase {
        running: art.visible && art.rainbow && art.effect.speed === true
        from: 0
        to: 1
        loops: Animation.Infinite
        duration: 12000 / (art.device.speed + 1)
    }

    function glow(x) {
        if (!lit || level <= 0)
            return "transparent";
        if (rainbow)
            return Qt.hsva((x / 879 + phase) % 1, 0.85, 1, 0.9 * level);
        const c = device.palette[device.colorSlot];
        return Qt.rgba(c.r, c.g, c.b, 0.9 * level);
    }

    implicitWidth: Kirigami.Units.gridUnit * 44
    implicitHeight: width * 399 / 879

    // ------------------------------------------------------------------ case
    Rectangle { // shell
        x: 2 * art.k
        y: 2 * art.k
        width: 875 * art.k
        height: 395 * art.k
        radius: 30 * art.k
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03)
        border.color: art.line
        border.width: 1.6
    }
    Rectangle { // bezel
        x: 10 * art.k
        y: 9 * art.k
        width: 859 * art.k
        height: 381 * art.k
        radius: 24 * art.k
        color: "transparent"
        border.color: art.soft
    }
    Rectangle { // key well
        x: 27 * art.k
        y: 45 * art.k
        width: 826 * art.k
        height: 333 * art.k
        radius: 12 * art.k
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.02)
        border.color: art.soft
    }
    Rectangle { // badge
        x: 34 * art.k
        y: 17 * art.k
        width: 66 * art.k
        height: 21 * art.k
        radius: 5 * art.k
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        border.color: art.line
        border.width: 1.2

        QQC2.Label {
            anchors.centerIn: parent
            text: "iLovbee"
            color: art.line
            font.bold: true
            font.pixelSize: Math.max(6, 10.5 * art.k)
        }
    }

    // ------------------------------------------------------------------ keys
    Repeater {
        model: art.device.keys
        delegate: QQC2.AbstractButton {
            id: cap
            required property var modelData
            readonly property bool remapped: modelData.type !== "factory"
            readonly property color glowColor: art.glow(modelData.x + modelData.w / 2)
            readonly property bool glowing: glowColor.a > 0

            x: modelData.x * art.k
            y: modelData.y * art.k
            width: modelData.w * art.k
            height: modelData.h * art.k
            hoverEnabled: true
            enabled: art.device.ready && !art.device.busy

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            QQC2.ToolTip.text: remapped ? i18nc("@info:tooltip key → assignment", "%1 → %2", modelData.label, modelData.text)
                                        : i18nc("@info:tooltip", "%1 (click to remap)", modelData.label)

            onClicked: art.keyClicked(modelData)

            background: Item {
                // underglow between the caps
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: cap.modelData.knob ? width / 2 : 8 * art.k + 2
                    color: "transparent"
                    border.width: 3
                    border.color: cap.glowColor
                    opacity: 0.45
                    visible: cap.glowing
                }
                // cap outline
                Rectangle {
                    anchors.fill: parent
                    radius: cap.modelData.knob ? width / 2 : 8 * art.k
                    color: cap.pressed ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.2)
                                       : "transparent"
                    border.width: cap.hovered || cap.visualFocus ? 2 : 1.4
                    border.color: cap.hovered || cap.visualFocus ? Kirigami.Theme.highlightColor
                                : cap.glowing ? Qt.tint(art.line, Qt.rgba(cap.glowColor.r, cap.glowColor.g, cap.glowColor.b, 0.7))
                                : art.line
                }
                // keycap top
                Rectangle {
                    x: parent.width * (cap.modelData.knob ? 0.12 : 0.09)
                    y: parent.height * (cap.modelData.knob ? 0.12 : 0.07)
                    width: parent.width * (cap.modelData.knob ? 0.76 : 0.82)
                    height: parent.height * (cap.modelData.knob ? 0.76 : 0.76)
                    radius: cap.modelData.knob ? width / 2 : 6 * art.k
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03)
                    border.color: art.soft
                    border.width: 1
                }
                // remapped marker
                Rectangle {
                    visible: cap.remapped
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: Math.max(2, 4 * art.k)
                    width: Math.max(4, 7 * art.k)
                    height: width
                    radius: width / 2
                    color: Kirigami.Theme.highlightColor
                }
            }

            contentItem: QQC2.Label {
                topPadding: cap.modelData.knob ? 0 : -cap.height * 0.1
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                font.pixelSize: Math.max(6, Math.min(12, 11 * art.k * 1.15))
                color: cap.remapped ? Kirigami.Theme.highlightColor : art.line
                font.bold: cap.remapped
                text: cap.remapped && cap.modelData.text.length <= 6 ? cap.modelData.text : cap.modelData.caption
            }
        }
    }
}
