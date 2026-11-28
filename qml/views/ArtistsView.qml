import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var lib
    property bool embedded: false

    property string selectedArtist: ""

    function selectArtist(name) {
        if (!name || name.length === 0) return
        root.selectedArtist = name
        root.lib.openArtist(name)
    }

    onSelectedArtistChanged: artistDetailAnim.restart()

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.embedded ? 0 : (root.width < 680 ? 16 : (root.width >= 1560 ? 26 : 22))
        anchors.topMargin: root.embedded ? 4 : anchors.margins
        spacing: 16

        // Left Spatial Glass Artist Rail
        GlassPanel {
            Layout.preferredWidth: Math.round(Math.max(200, Math.min(340, root.width * 0.24)))
            Layout.fillHeight: true
            radius: DesignTokens.radiusLg
            panelColor: DesignTokens.stageSurfaceBg
            borderColor: DesignTokens.hairlineBorder
            shadowEnabled: false

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Artists")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        font.letterSpacing: -0.2
                    }

                    Rectangle {
                        implicitWidth: arCntTxt.implicitWidth + 14
                        implicitHeight: 22
                        radius: 11
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: DesignTokens.hairlineBorder

                        Text {
                            id: arCntTxt
                            anchors.centerIn: parent
                            text: String(root.lib.artistsModel.count)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            font.features: ({ "tnum": 1 })
                        }
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

                        Rectangle {
                            visible: arRow.sel
                            width: 3
                            height: 16
                            radius: 1.5
                            anchors.left: parent.left
                            anchors.leftMargin: 3
                            anchors.verticalCenter: parent.verticalCenter
                            color: DesignTokens.accent
                        }

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
        Item {
            id: rightPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            transform: Translate { id: rightShift; y: 0 }

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

            ParallelAnimation {
                id: artistDetailAnim
                NumberAnimation { target: rightPane; property: "opacity"; from: 0.0; to: 1.0; duration: 190; easing.type: Easing.OutCubic }
                NumberAnimation { target: rightShift; property: "y"; from: 10; to: 0; duration: 240; easing.type: Easing.OutQuint }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: 74

                Rectangle {
                    anchors.fill: parent
                    radius: DesignTokens.radiusLg
                    visible: root.selectedArtist.length > 0
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: DesignTokens.accentSoft }
                        GradientStop { position: 0.65; color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.03) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: DesignTokens.hairlineBorder
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.selectedArtist.length > 0 ? 14 : 0
                    anchors.rightMargin: 8
                    spacing: 14

                    Rectangle {
                        visible: root.selectedArtist.length > 0
                        implicitWidth: 46
                        implicitHeight: 46
                        radius: 23
                        color: DesignTokens.accentSoft
                        border.width: 1
                        border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.34)

                        Text {
                            anchors.centerIn: parent
                            text: root.selectedArtist.trim().length > 0 ? root.selectedArtist.trim().charAt(0).toUpperCase() : ""
                            color: DesignTokens.accentLight
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 18
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedArtist.length > 0 ? root.selectedArtist : qsTr("Select an Artist")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 24
                            font.weight: Font.Bold
                            font.letterSpacing: -0.3
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: root.selectedArtist.length > 0
                            text: qsTr("%1 track(s)").arg(root.lib.detailTracksModel.count)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    VirelaButton {
                        visible: root.selectedArtist.length > 0 && root.lib.detailTracksModel.count > 0
                        primary: true
                        iconName: "play"
                        text: qsTr("Play All")
                        onClicked: root.lib.playFromDetailModel(0)
                    }
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

                    Rectangle {
                        visible: atRow.isCurrent
                        width: 3
                        height: 18
                        radius: 1.5
                        anchors.left: parent.left
                        anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: DesignTokens.accent
                    }

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
                                controller: root.controller
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
}
