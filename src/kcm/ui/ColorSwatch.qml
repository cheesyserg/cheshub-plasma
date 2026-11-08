// SPDX-License-Identifier: GPL-3.0-or-later
// A colour square that opens a colour dialog. Emits picked(color).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

QQC2.AbstractButton {
    id: swatch

    property color color
    property bool selected: false
    property string dialogTitle: i18nc("@title:window", "Choose Colour")
    signal picked(color color)

    implicitWidth: Kirigami.Units.gridUnit * 2
    implicitHeight: Kirigami.Units.gridUnit * 1.5
    hoverEnabled: true

    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.text: swatch.color.toString().toUpperCase()

    background: Rectangle {
        radius: Kirigami.Units.smallSpacing
        color: swatch.color
        border.width: swatch.selected ? 3 : 1
        border.color: swatch.selected ? Kirigami.Theme.highlightColor
            : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.3)
    }

    onClicked: dialog.open()

    ColorDialog {
        id: dialog
        title: swatch.dialogTitle
        selectedColor: swatch.color
        onAccepted: swatch.picked(selectedColor)
    }
}
