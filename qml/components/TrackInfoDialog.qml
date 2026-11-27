import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Virela.UI

Item {
    id: root

    property var trackMap: ({})
    property bool open: true
    property real animProgress: 0.0
    signal closeRequested()
    signal closed()

    anchors.fill: parent

    Component.onCompleted: {
        if (root.open) {
            Qt.callLater(function() {
                if (root.open) openAnim.restart()
            })
        }
    }

    onOpenChanged: {
        if (open) {
            closeAnim.stop()
            openAnim.restart()
        } else {
            openAnim.stop()
            closeAnim.restart()
        }
    }

    NumberAnimation {
        id: openAnim
        target: root
        property: "animProgress"
        to: 1.0
        duration: 210
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: closeAnim
        target: root
        property: "animProgress"
        to: 0.0
        duration: 150
        easing.type: Easing.InQuad
        onFinished: {
            if (!root.open) root.closed()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: DesignTokens.surfaceScrim
        opacity: root.animProgress
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
        }
    }

    GlassPanel {
        id: dialogCard
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 512)
        height: Math.min(parent.height - 64, infoCol.implicitHeight + 44)
        radius: 22
        panelColor: DesignTokens.dockFloatingBg
        borderColor: DesignTokens.borderHighlight
        elevated: true
        shadowEnabled: true
        specularEnabled: true
        opacity: root.animProgress
        scale: 0.95 + 0.05 * root.animProgress
        transform: Translate { y: (1.0 - root.animProgress) * 12 }

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
                    font.features: ({ "tnum": 1 })
                }
            }

            Rectangle {
                visible: showDivider
                Layout.fillWidth: true
                implicitHeight: 1
                color: DesignTokens.hairlineBorder
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
                spacing: 12

                Rectangle {
                    implicitWidth: 52
                    implicitHeight: 52
                    radius: DesignTokens.radiusMd
                    color: DesignTokens.surfaceInput
                    border.width: 1
                    border.color: DesignTokens.borderHighlight
                    clip: true

                    Rectangle {
                        id: infoCoverMask
                        anchors.fill: parent
                        radius: DesignTokens.radiusMd
                        visible: false
                        layer.enabled: true
                        layer.smooth: true
                    }

                    Image {
                        id: infoCoverImg
                        anchors.fill: parent
                        source: (root.trackMap.coverHash && root.trackMap.coverHash.length > 0)
                            ? ("image://cover/" + root.trackMap.coverHash + "?size=160") : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                        visible: false
                    }

                    MultiEffect {
                        anchors.fill: infoCoverImg
                        visible: infoCoverImg.status === Image.Ready
                        source: infoCoverImg
                        maskEnabled: true
                        maskSource: infoCoverMask
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }

                    IconGlyph {
                        anchors.centerIn: parent
                        name: "music"
                        size: 22
                        color: DesignTokens.accent
                        visible: infoCoverImg.status !== Image.Ready
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: root.trackMap.title || qsTr("Track Technical Info")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: root.trackMap.artist || qsTr("Track Technical Info")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
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
                border.color: DesignTokens.hairlineBorder

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
