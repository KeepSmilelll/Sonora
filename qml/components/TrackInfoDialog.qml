import QtQuick
import QtQuick.Layouts
import Sonora.UI

Item {
    id: root

    property var trackMap: ({})
    signal closeRequested()

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: DesignTokens.surfaceScrim
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
        }
    }

    GlassPanel {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 504)
        height: Math.min(parent.height - 64, infoCol.implicitHeight + 44)
        radius: DesignTokens.radiusXl
        elevated: true
        shadowEnabled: true

        MouseArea { anchors.fill: parent }

        component SpecRow: ColumnLayout {
            property string label: ""
            property string value: ""
            property bool showDivider: true
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.preferredWidth: 116
                    text: label
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Text {
                    Layout.fillWidth: true
                    text: value.length > 0 ? value : "—"
                    elide: Text.ElideMiddle
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }

            Rectangle {
                visible: showDivider
                Layout.fillWidth: true
                implicitHeight: 1
                color: DesignTokens.borderSubtle
            }
        }

        ColumnLayout {
            id: infoCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    implicitWidth: 30
                    implicitHeight: 30
                    radius: 9
                    color: DesignTokens.accentSoft
                    IconGlyph { anchors.centerIn: parent; name: "info"; size: 15; color: DesignTokens.accent }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Track Technical Info")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }

                FormatBadge {
                    visible: Boolean(root.trackMap.formatBadge)
                    compact: true
                    text: root.trackMap.formatBadge || ""
                }

                FormatBadge {
                    visible: Boolean(root.trackMap.isCue)
                    compact: true
                    text: "CUE"
                }

                Rectangle {
                    implicitWidth: 28
                    implicitHeight: 28
                    radius: 8
                    color: clMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    IconGlyph {
                        anchors.centerIn: parent
                        name: "close"
                        size: 13
                        color: clMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                    }
                    MouseArea {
                        id: clMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeRequested()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: specList.implicitHeight + 24
                radius: DesignTokens.radiusMd
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: DesignTokens.borderOuter

                ColumnLayout {
                    id: specList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    SpecRow { label: qsTr("Title"); value: root.trackMap.title || "" }
                    SpecRow { label: qsTr("Artist"); value: root.trackMap.artist || "" }
                    SpecRow { label: qsTr("Album"); value: root.trackMap.album || "" }
                    SpecRow { label: qsTr("Codec / Format"); value: (root.trackMap.codec || "") + (root.trackMap.formatBadge ? (" (" + root.trackMap.formatBadge + ")") : "") }
                    SpecRow { label: qsTr("Audio Specs"); value: root.trackMap.techSummary || "" }
                    SpecRow { label: qsTr("Bitrate"); value: root.trackMap.bitrateKbps > 0 ? (root.trackMap.bitrateKbps + " kbps") : "" }
                    SpecRow {
                        label: qsTr("ReplayGain")
                        value: root.trackMap.hasRgTrack
                            ? (qsTr("Track %1 dB (Peak %2)").arg(Number(root.trackMap.rgTrackGainDb).toFixed(2)).arg(Number(root.trackMap.rgTrackPeak).toFixed(3)))
                            : qsTr("Not Tagged")
                    }
                    SpecRow {
                        visible: Boolean(root.trackMap.isCue)
                        label: qsTr("CUE Sheet")
                        value: root.trackMap.cuePath || ""
                    }
                    SpecRow {
                        label: qsTr("File Path")
                        value: root.trackMap.filePath || ""
                        showDivider: false
                    }
                }
            }
        }
    }
}
