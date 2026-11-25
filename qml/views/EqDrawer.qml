import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

// 10-band Parametric Equalizer + Real-time GPU Curve + Quick Audio DSP Drawer (lazy-loaded)
Rectangle {
    id: root

    required property var controller
    required property var settingsObj
    signal closeRequested()

    width: 480
    height: 410
    radius: DesignTokens.radiusXl
    color: DesignTokens.surfaceElevated
    border.width: 1
    border.color: DesignTokens.borderHighlight

    property int eqRev: 0
    readonly property var bandLabels: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

    Connections {
        target: root.controller
        function onEqCurveChanged() { root.eqRev += 1 }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Equalizer & DSP")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 15
                font.weight: Font.Bold
            }

            Switch {
                checked: root.settingsObj.eqEnabled
                onToggled: root.settingsObj.eqEnabled = checked
            }

            Item { Layout.fillWidth: true }

            ComboBox {
                id: presetBox
                Layout.preferredWidth: 132
                model: root.controller.eqPresets
                textRole: "name"
                onActivated: (idx) => {
                    const p = root.controller.eqPresets[idx]
                    if (p) root.settingsObj.applyEqPreset(p.id)
                }
            }

            Rectangle {
                implicitWidth: 26; implicitHeight: 26; radius: 7
                color: closeMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "close"; size: 12; color: DesignTokens.textSecondary }
                MouseArea {
                    id: closeMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }

        // GPU Frequency Response Curve Preview
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 92
            radius: 10
            color: DesignTokens.surfaceCard
            border.width: 1
            border.color: DesignTokens.borderSubtle
            clip: true

            // 0 dB center reference line
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: DesignTokens.borderSubtle
            }

            EqCurveItem {
                anchors.fill: parent
                lineColor: root.settingsObj.eqEnabled ? DesignTokens.accent : DesignTokens.textTertiary
                revision: root.eqRev
            }

            Text {
                anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 6
                text: "+12 dB"; color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily; font.pixelSize: 10
            }
            Text {
                anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 6
                text: "-12 dB"; color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily; font.pixelSize: 10
            }
        }

        // 10-band vertical sliders
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            Repeater {
                model: 10
                delegate: ColumnLayout {
                    id: bandCol
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    readonly property real bandVal: root.settingsObj.eqBands[bandCol.index] !== undefined
                        ? root.settingsObj.eqBands[bandCol.index] : 0.0

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: (bandCol.bandVal >= 0 ? "+" : "") + bandCol.bandVal.toFixed(0)
                        color: Math.abs(bandCol.bandVal) > 0.1 ? DesignTokens.accent : DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 10
                    }

                    Slider {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillHeight: true
                        orientation: Qt.Vertical
                        from: -12.0
                        to: 12.0
                        stepSize: 0.5
                        value: bandCol.bandVal
                        enabled: root.settingsObj.eqEnabled
                        onMoved: root.settingsObj.setEqBandGain(bandCol.index, value)
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.bandLabels[bandCol.index]
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 10
                    }
                }
            }
        }

        // Bottom strip: Preamp + ReplayGain quick toggle + WASAPI Exclusive indicator
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("Preamp: %1 dB").arg(root.settingsObj.eqPreampDb.toFixed(1))
                color: DesignTokens.textSecondary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
            }

            Slider {
                Layout.preferredWidth: 110
                from: -12.0; to: 12.0; stepSize: 0.5
                value: root.settingsObj.eqPreampDb
                enabled: root.settingsObj.eqEnabled
                onMoved: root.settingsObj.eqPreampDb = value
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                implicitHeight: 26; implicitWidth: flatLbl.implicitWidth + 16; radius: 13
                color: flatMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                Text {
                    id: flatLbl; anchors.centerIn: parent; text: qsTr("Reset Flat")
                    color: DesignTokens.textPrimary; font.family: DesignTokens.fontFamily; font.pixelSize: 11
                }
                MouseArea {
                    id: flatMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.settingsObj.applyEqPreset("flat")
                }
            }
        }
    }
}
