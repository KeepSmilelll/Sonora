import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    property var trackMap: ({})
    signal closeRequested()

    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.45)

    MouseArea {
        anchors.fill: parent
        onClicked: root.closeRequested()
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 480)
        height: Math.min(parent.height - 64, infoCol.implicitHeight + 40)
        radius: DesignTokens.radiusXl
        color: DesignTokens.surfaceElevated
        border.width: 1
        border.color: DesignTokens.borderHighlight

        MouseArea { anchors.fill: parent } // eat clicks inside dialog

        component SpecRow: RowLayout {
            property string label: ""
            property string value: ""
            Layout.fillWidth: true
            spacing: 12
            Text {
                Layout.preferredWidth: 112
                text: label
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 12
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

        ColumnLayout {
            id: infoCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Track Technical Info")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }
                Rectangle {
                    implicitWidth: 28; implicitHeight: 28; radius: 8
                    color: clMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    IconGlyph { anchors.centerIn: parent; name: "close"; size: 13; color: DesignTokens.textSecondary }
                    MouseArea {
                        id: clMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeRequested()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: DesignTokens.borderSubtle }

            SpecRow { label: qsTr("Title"); value: root.trackMap.title || "" }
            SpecRow { label: qsTr("Artist"); value: root.trackMap.artist || "" }
            SpecRow { label: qsTr("Album"); value: root.trackMap.album || "" }
            SpecRow { label: qsTr("Codec / Format"); value: (root.trackMap.codec || "") + " (" + (root.trackMap.formatBadge || "") + ")" }
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
            SpecRow { label: qsTr("File Path"); value: root.trackMap.filePath || "" }
        }
    }
}
