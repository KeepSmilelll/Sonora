import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib

    property string selectedArtist: ""

    onSelectedArtistChanged: artistDetailAnim.restart()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 16

        // Left Spatial Glass Artist Rail
        GlassPanel {
            Layout.preferredWidth: 258
            Layout.fillHeight: true
            radius: DesignTokens.radiusLg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    spacing: 8

                    IconGlyph { name: "artist"; size: 16; color: DesignTokens.accent }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Artists (%1)").arg(root.lib.artistsModel.count)
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 3
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.lib.artistsModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        id: arRow
                        required property string artist
                        required property int albumCount
                        required property int trackCount

                        readonly property bool sel: root.selectedArtist === artist
                        readonly property string initial: artist.trim().length > 0 ? artist.trim().charAt(0).toUpperCase() : "♪"
                        width: ListView.view ? ListView.view.width : 220
                        height: 48
                        radius: 10
                        color: sel
                            ? DesignTokens.surfaceSelected
                            : (arMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                        border.width: sel ? 1 : 0
                        border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                implicitWidth: 32
                                implicitHeight: 32
                                radius: 16
                                color: arRow.sel ? DesignTokens.accent : DesignTokens.accentSoft

                                Text {
                                    anchors.centerIn: parent
                                    text: arRow.initial
                                    color: arRow.sel ? "#ffffff" : DesignTokens.accent
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: arRow.artist
                                    elide: Text.ElideRight
                                    color: arRow.sel ? DesignTokens.accent : DesignTokens.textPrimary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 13
                                    font.weight: arRow.sel ? Font.DemiBold : Font.Medium
                                }
                                Text {
                                    text: qsTr("%1 album(s) · %2 track(s)").arg(arRow.albumCount).arg(arRow.trackCount)
                                    color: DesignTokens.textTertiary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 11
                                }
                            }
                        }

                        MouseArea {
                            id: arMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedArtist = arRow.artist
                                root.lib.openArtist(arRow.artist)
                            }
                        }
                    }
                }
            }
        }

        // Right Artist Tracks Pane
        ColumnLayout {
            id: rightPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            transform: Translate { id: rightShift; y: 0 }

            ParallelAnimation {
                id: artistDetailAnim
                NumberAnimation { target: rightPane; property: "opacity"; from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { target: rightShift; property: "y"; from: 8; to: 0; duration: 220; easing.type: Easing.OutQuint }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    visible: root.selectedArtist.length > 0
                    implicitWidth: 42
                    implicitHeight: 42
                    radius: 21
                    color: DesignTokens.accentSoft
                    border.width: 1
                    border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)

                    Text {
                        anchors.centerIn: parent
                        text: root.selectedArtist.trim().length > 0 ? root.selectedArtist.trim().charAt(0).toUpperCase() : ""
                        color: DesignTokens.accent
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedArtist.length > 0 ? root.selectedArtist : qsTr("Select an Artist")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: root.selectedArtist.length > 0
                        text: qsTr("%1 track(s)").arg(root.lib.detailTracksModel.count)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }

                SonoraButton {
                    visible: root.selectedArtist.length > 0 && root.lib.detailTracksModel.count > 0
                    primary: true
                    iconName: "play"
                    text: qsTr("Play All")
                    onClicked: root.lib.playFromDetailModel(0)
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.lib.detailTracksModel
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: atRow
                    required property int index
                    required property int trackId
                    required property string title
                    required property string album
                    required property int durationMs
                    required property string formatBadge

                    readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                    width: ListView.view ? ListView.view.width : 400
                    height: 48
                    radius: 10
                    color: isCurrent
                        ? DesignTokens.surfaceSelected
                        : (atMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                    border.width: isCurrent ? 1 : 0
                    border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                    Behavior on color { ColorAnimation { duration: 110 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 14
                        spacing: 12

                        Item {
                            Layout.preferredWidth: 24
                            implicitHeight: 20

                            NowPlayingBars {
                                anchors.centerIn: parent
                                visible: atRow.isCurrent
                                playing: root.controller.isPlaying
                                size: 13
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !atRow.isCurrent
                                text: String(atRow.index + 1)
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                                font.features: ({ "tnum": 1 })
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: atRow.title
                                elide: Text.ElideRight
                                color: atRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: atRow.isCurrent ? Font.DemiBold : Font.Medium
                            }
                            Text {
                                Layout.fillWidth: true
                                text: atRow.album
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        FormatBadge { compact: true; text: atRow.formatBadge }

                        Text {
                            Layout.preferredWidth: 48
                            horizontalAlignment: Text.AlignRight
                            text: root.controller.formatTime(atRow.durationMs)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    MouseArea {
                        id: atMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.playFromDetailModel(atRow.index)
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    visible: root.selectedArtist.length === 0
                    spacing: 8
                    IconGlyph {
                        Layout.alignment: Qt.AlignHCenter
                        name: "artist"; size: 36; color: DesignTokens.textTertiary
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Choose an artist on the left to browse tracks")
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
