// SPDX-License-Identifier: GPL-3.0-or-later
// Line-art TK-GM012 (top view, traced from the product render in the web
// studio) with a callout per button, like the web studio's anchors. Colours
// come from the Kirigami theme so it follows light and dark themes.
//
// Drawing coordinates are 260 x 500; `s` scales them to the screen.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

Item {
    id: art

    required property var device

    readonly property real s: Kirigami.Units.gridUnit / 20
    readonly property real calloutWidth: Kirigami.Units.gridUnit * 10
    readonly property real gap: Kirigami.Units.gridUnit * 2.5
    readonly property real bodyWidth: 260 * s
    readonly property real bodyHeight: 500 * s
    // Below this width the callouts are hidden and the page lists the buttons
    // in its form instead.
    readonly property real fullWidth: calloutWidth * 2 + gap * 2 + bodyWidth
    readonly property bool compact: width < fullWidth
    // The callouts and the mouse are centred as one group.
    readonly property real originX: compact ? 0 : (width - fullWidth) / 2
    readonly property real bodyX: compact ? (width - bodyWidth) / 2 : originX + calloutWidth + gap

    readonly property color line: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.7)
    readonly property color soft: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.3)
    readonly property color ledColor: device.stageColors[device.activeStage] || Kirigami.Theme.highlightColor

    // Callout per button, in device.buttons order (visual order):
    // side, callout centre y and the point it points at, in drawing units.
    readonly property var callouts: [
        { side: "left", y: 80, tx: 70, ty: 96 }, // left button
        { side: "right", y: 60, tx: 190, ty: 96 }, // right button
        { side: "right", y: 150, tx: 150, ty: 104 }, // wheel
        { side: "left", y: 170, tx: 14, ty: 170 }, // front side button
        { side: "left", y: 260, tx: 14, ty: 240 }, // rear side button
        { side: "right", y: 240, tx: 144, ty: 183, dashed: true } // DPI button
    ]

    implicitWidth: fullWidth
    implicitHeight: bodyHeight

    // ------------------------------------------------------------- the mouse
    Item {
        id: body
        x: art.bodyX
        width: 260
        height: 500
        scale: art.s
        transformOrigin: Item.TopLeft

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Shell
            ShapePath {
                strokeColor: art.line
                strokeWidth: 1.8 / art.s
                fillColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03)
                PathSvg {
                    path: "M 130 14 C 192 14 236 42 238 122 C 240 200 228 250 232 300 C 238 362 246 420 222 462 "
                        + "C 200 496 60 496 38 462 C 14 420 22 362 28 300 C 32 250 20 200 22 122 C 24 42 68 14 130 14 Z"
                }
            }
            // Button split: across the shell, and down the middle
            ShapePath {
                strokeColor: art.soft
                strokeWidth: 1.2 / art.s
                fillColor: "transparent"
                PathSvg {
                    path: "M 27 236 C 80 252 180 252 233 236 M 130 14 L 130 56 M 130 220 L 130 246"
                }
            }
            // Wheel well and wheel
            ShapePath {
                strokeColor: art.line
                strokeWidth: 1.4 / art.s
                fillColor: "transparent"
                PathSvg {
                    path: "M 112 72 Q 112 56 130 56 Q 148 56 148 72 L 148 128 Q 148 144 130 144 Q 112 144 112 128 Z"
                }
            }
            ShapePath {
                strokeColor: art.soft
                strokeWidth: 1 / art.s
                fillColor: "transparent"
                PathSvg {
                    path: "M 119 70 L 141 70 M 118 80 L 142 80 M 118 90 L 142 90 M 118 100 L 142 100 "
                        + "M 118 110 L 142 110 M 118 120 L 142 120 M 119 130 L 141 130"
                }
            }
            // DPI button
            ShapePath {
                strokeColor: art.line
                strokeWidth: 1.4 / art.s
                fillColor: "transparent"
                PathSvg {
                    path: "M 118 160 Q 118 152 126 152 L 134 152 Q 142 152 142 160 L 142 208 Q 142 216 134 216 "
                        + "L 126 216 Q 118 216 118 208 Z"
                }
            }
            // Side buttons (front, rear)
            ShapePath {
                strokeColor: art.line
                strokeWidth: 1.4 / art.s
                fillColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05)
                PathSvg {
                    path: "M 22 140 C 12 142 10 150 10 170 C 10 190 12 198 22 200 "
                        + "M 22 210 C 11 212 9 222 9 240 C 9 258 11 268 22 270"
                }
            }
            // Badge
            ShapePath {
                strokeColor: art.soft
                strokeWidth: 1.2 / art.s
                fillColor: "transparent"
                PathSvg {
                    path: "M 98 392 L 130 410 L 162 392 M 106 404 L 130 418 L 154 404 M 116 414 L 130 424 L 144 414 M 130 404 L 130 432"
                }
            }
        }

        // DPI indicator LED in the active stage's colour.
        Rectangle {
            x: 130 - width / 2
            y: 184 - height / 2
            width: 7
            height: 14
            radius: 3.5
            color: art.ledColor
        }
        Rectangle {
            x: 130 - width / 2
            y: 184 - height / 2
            width: 22
            height: 30
            radius: 11
            color: art.ledColor
            opacity: 0.22
        }
    }

    // ------------------------------------------------------------- callouts
    Repeater {
        model: art.compact ? [] : art.device.buttons
        delegate: Item {
            id: callout
            required property int index
            required property var modelData
            readonly property var spec: art.callouts[index]
            readonly property bool onLeft: spec.side === "left"
            readonly property real cy: spec.y * art.s
            readonly property real edgeX: onLeft ? art.originX + art.calloutWidth : art.bodyX + art.bodyWidth + art.gap
            readonly property real targetX: art.bodyX + spec.tx * art.s
            readonly property real targetY: spec.ty * art.s

            anchors.fill: parent

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: picker.hovered || picker.pressed ? Kirigami.Theme.highlightColor : art.soft
                    strokeWidth: 1.2
                    strokeStyle: callout.spec.dashed ? ShapePath.DashLine : ShapePath.SolidLine
                    dashPattern: [4, 3]
                    fillColor: "transparent"
                    startX: callout.edgeX + (callout.onLeft ? Kirigami.Units.smallSpacing : -Kirigami.Units.smallSpacing)
                    startY: callout.cy
                    PathLine {
                        x: callout.targetX
                        y: callout.targetY
                    }
                }
            }

            Rectangle {
                x: callout.targetX - width / 2
                y: callout.targetY - height / 2
                width: 7
                height: 7
                radius: 3.5
                color: picker.hovered || picker.pressed ? Kirigami.Theme.highlightColor : art.line
            }

            ColumnLayout {
                x: callout.onLeft ? art.originX : callout.edgeX
                y: callout.cy - height / 2
                width: art.calloutWidth
                spacing: 0

                QQC2.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: callout.onLeft ? Text.AlignRight : Text.AlignLeft
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.75
                    text: callout.modelData.label
                }
                QQC2.ComboBox {
                    id: picker
                    Layout.fillWidth: true
                    model: (callout.modelData.custom ? [{ id: "custom", label: callout.modelData.actionLabel }] : []).concat(art.device.actions)
                    textRole: "label"
                    valueRole: "id"
                    currentIndex: model.findIndex(a => a.id === callout.modelData.action)
                    onActivated: idx => art.device.setButtonAction(callout.index, model[idx].id)
                }
            }
        }
    }
}
