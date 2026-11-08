// SPDX-License-Identifier: GPL-3.0-or-later
// TECKNET TK-GM012. Mirrors the web studio's feature set, which comes from the
// vendor descriptor: report rate, six DPI stages with indicator colours,
// six button bindings, sleep timeout and move wake-up. No lighting page,
// lift-off distance or debounce: the mouse does not expose them.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: page

    required property var device
    // The page never takes its width from its content (that fed back into the
    // layout and froze System Settings); the column is centred and capped so
    // it stays readable in a maximized window.
    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 50)
        spacing: Kirigami.Units.largeSpacing * 2

        DeviceHeader {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.largeSpacing
            device: page.device
        }

        MouseArt {
            id: art
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            enabled: page.device.ready && !page.device.busy
            device: page.device
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
            enabled: page.device.ready && !page.device.busy

            // ---------------------------------------------------------------- DPI
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Sensitivity")
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "DPI stages in use:")
                from: 1
                to: 6
                value: page.device.stageCount
                onValueModified: page.device.stageCount = value
            }

            Repeater {
                model: 6
                delegate: RowLayout {
                    id: stage
                    required property int index
                    readonly property int dpi: page.device.dpi[index]

                    Kirigami.FormData.label: i18nc("@label DPI stage number", "Stage %1:", index + 1)
                    enabled: index < page.device.stageCount
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.RadioButton {
                        checked: page.device.activeStage === stage.index
                        onToggled: page.device.activeStage = stage.index
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: i18nc("@info:tooltip", "Make this the active stage")
                    }

                    QQC2.Slider {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                        from: page.device.dpiMin
                        to: page.device.dpiMax
                        // Codec steps: 50 up to 10000, 100 up to 12000, then 1000.
                        stepSize: value < 10000 ? 50 : (value < 13000 ? 100 : 1000)
                        value: stage.dpi
                        live: false
                        onMoved: page.device.setDpi(stage.index, page.device.snapDpi(value))
                    }

                    QQC2.SpinBox {
                        from: page.device.dpiMin
                        to: page.device.dpiMax
                        editable: true
                        stepSize: value < 10000 ? 50 : (value < 13000 ? 100 : 1000)
                        value: stage.dpi
                        textFromValue: (v, locale) => i18nc("@item DPI value", "%1 DPI", v)
                        valueFromText: (text, locale) => parseInt(text) || stage.dpi
                        onValueModified: page.device.setDpi(stage.index, value)
                    }

                    ColorSwatch {
                        color: page.device.stageColors[stage.index]
                        dialogTitle: i18nc("@title:window", "Indicator Colour for Stage %1", stage.index + 1)
                        onPicked: c => page.device.setStageColor(stage.index, c)
                        QQC2.ToolTip.text: i18nc("@info:tooltip", "Colour of the DPI indicator for this stage")
                    }
                }
            }

            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                opacity: 0.8
                text: i18n("The DPI button cycles through the stages in use. Values between 12000 and 13000 are skipped: the mouse cannot store them distinctly.")
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "Report rate:")
                model: page.device.rates.map(hz => i18nc("@item:inlistbox polling rate", "%1 Hz", hz))
                currentIndex: page.device.rates.indexOf(page.device.rate)
                onActivated: index => page.device.rate = page.device.rates[index]
            }

            // ------------------------------------------- buttons (narrow windows)
            Kirigami.Separator {
                visible: art.compact
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Buttons")
            }

            Repeater {
                model: art.compact ? page.device.buttons : []
                delegate: QQC2.ComboBox {
                    id: binding
                    required property int index
                    required property var modelData

                    Kirigami.FormData.label: i18nc("@label:listbox button name", "%1:", modelData.label)
                    model: (modelData.custom ? [{ id: "custom", label: modelData.actionLabel }] : []).concat(page.device.actions)
                    textRole: "label"
                    valueRole: "id"
                    currentIndex: model.findIndex(a => a.id === modelData.action)
                    onActivated: idx => page.device.setButtonAction(binding.index, model[idx].id)
                }
            }

            // ------------------------------------------------------------- power
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Power")
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "Sleep after:")
                model: page.device.sleepOptions.map(m => i18np("%1 minute", "%1 minutes", m))
                currentIndex: page.device.sleepOptions.indexOf(page.device.sleepMinutes)
                onActivated: index => page.device.sleepMinutes = page.device.sleepOptions[index]
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18nc("@label:checkbox", "Wake up:")
                text: i18nc("@option:check", "When the mouse is moved")
                checked: page.device.moveWake
                onToggled: page.device.moveWake = checked
            }
        }
    }
}
