import QtQuick
import QtQuick.Layouts
import Virela.UI

// 10-band Parametric Equalizer + Real-time GPU Curve + Quick Audio DSP Spatial Drawer
GlassPanel {
    id: root

    required property var controller
    required property var settingsObj
    signal closeRequested()

    implicitWidth: 536
    implicitHeight: 404
    radius: 22
    panelColor: DesignTokens.dockFloatingBg
    borderColor: DesignTokens.borderHighlight
    elevated: true
    shadowEnabled: true
    specularEnabled: true

    property int eqRev: 0
    readonly property var bandLabels: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

    Connections {
        target: root.controller
        function onEqCurveChanged() { root.eqRev += 1 }
    }

    // Consume clicks inside the drawer so click-outside catcher does not dismiss it
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                implicitWidth: 30
                implicitHeight: 30
                radius: 9
                color: DesignTokens.accentSoft
                IconGlyph { anchors.centerIn: parent; name: "eq"; size: 15; color: DesignTokens.accent }
            }

            Text {
                text: qsTr("Equalizer & DSP")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 15
                font.weight: Font.Bold
            }

            VirelaSwitch {
                checked: root.settingsObj.eqEnabled
                onToggled: (state) => root.settingsObj.eqEnabled = state
            }

            Item { Layout.fillWidth: true }

            VirelaComboBox {
                id: presetBox
                Layout.preferredWidth: 148
                model: root.controller.eqPresets
                textRole: "name"
                onActivated: (idx) => {
                    const p = root.controller.eqPresets[idx]
                    if (p) root.settingsObj.applyEqPreset(p.id)
                }
            }

            Rectangle {
                implicitWidth: 28
                implicitHeight: 28
                radius: 8
                color: closeMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                scale: closeMa.pressed ? 0.90 : 1.0
                Behavior on scale { NumberAnimation { duration: 120 } }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "close"
                    size: 13
                    color: closeMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                }
                MouseArea {
                    id: closeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }

        // GPU Frequency Response Curve Preview
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 88
            radius: 12
            color: root.settingsObj.eqEnabled
                ? Qt.tint(DesignTokens.surfaceInput, Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.05))
                : DesignTokens.surfaceInput
            border.width: 1
            border.color: DesignTokens.hairlineBorder
            clip: true

            // +6 / 0 / -6 dB reference hairlines
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right
                y: Math.round(parent.height * 0.25)
                height: 1
                color: DesignTokens.borderSubtle
                opacity: 0.55
            }
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: DesignTokens.borderHighlight
                opacity: 0.65
            }
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right
                y: Math.round(parent.height * 0.75)
                height: 1
                color: DesignTokens.borderSubtle
                opacity: 0.55
            }

            EqCurveItem {
                anchors.fill: parent
                lineColor: root.settingsObj.eqEnabled ? DesignTokens.accent : DesignTokens.textTertiary
                revision: root.eqRev
            }

            Text {
                anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 7
                text: "+12 dB"
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.features: ({ "tnum": 1 })
            }
            Text {
                anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7
                text: "-12 dB"
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.features: ({ "tnum": 1 })
            }
        }

        // 10-band vertical bipolar spatial sliders
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
                        text: (bandCol.bandVal > 0.05 ? "+" : "") + bandCol.bandVal.toFixed(1)
                        color: Math.abs(bandCol.bandVal) > 0.05 ? DesignTokens.accent : DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 10
                        font.weight: Math.abs(bandCol.bandVal) > 0.05 ? Font.DemiBold : Font.Normal
                        font.features: ({ "tnum": 1 })
                    }

                    VirelaSlider {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillHeight: true
                        orientation: Qt.Vertical
                        bipolar: true
                        from: -12.0
                        to: 12.0
                        stepSize: 0.5
                        value: bandCol.bandVal
                        enabled: root.settingsObj.eqEnabled
                        onMoved: (v) => root.settingsObj.setEqBandGain(bandCol.index, v)
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.bandLabels[bandCol.index]
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }
                }
            }
        }

        // Bottom strip: Bipolar Preamp slider + Reset Flat button
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                Layout.preferredWidth: 102
                text: qsTr("Preamp: %1%2 dB")
                    .arg(root.settingsObj.eqPreampDb > 0.05 ? "+" : "")
                    .arg(root.settingsObj.eqPreampDb.toFixed(1))
                color: DesignTokens.textSecondary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
                font.weight: Font.Medium
                font.features: ({ "tnum": 1 })
            }

            VirelaSlider {
                Layout.preferredWidth: 136
                bipolar: true
                from: -12.0
                to: 12.0
                stepSize: 0.5
                value: root.settingsObj.eqPreampDb
                enabled: root.settingsObj.eqEnabled
                onMoved: (v) => root.settingsObj.eqPreampDb = v
            }

            Item { Layout.fillWidth: true }

            VirelaButton {
                compact: true
                text: qsTr("Reset Flat")
                onClicked: {
                    presetBox.currentIndex = 0
                    root.settingsObj.applyEqPreset("flat")
                }
            }
        }
    }
}
