import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib

    property bool inDetail: false
    property string selectedAlbum: ""
    property string selectedArtist: ""
    property string selectedCoverHash: ""
    property int selectedYear: 0

    onInDetailChanged: {
        subviewAnim.fromX = inDetail ? 22 : -22
        subviewAnim.restart()
    }

    ParallelAnimation {
        id: subviewAnim
        property real fromX: 22
        NumberAnimation {
            target: root.inDetail ? detailPane : gridPane
            property: "opacity"
            from: 0.0; to: 1.0
            duration: 210
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root.inDetail ? detailShift : gridShift
            property: "x"
            from: subviewAnim.fromX; to: 0
            duration: 240
            easing.type: Easing.OutQuint
        }
    }

    // Album Grid View
    ColumnLayout {
        id: gridPane
        visible: !root.inDetail
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14
        transform: Translate { id: gridShift; x: 0 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 13

            Rectangle {
                implicitWidth: 42
                implicitHeight: 42
                radius: 13
                color: DesignTokens.accentSoft
                border.width: 1
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                IconGlyph { anchors.centerIn: parent; name: "album"; size: 19; color: DesignTokens.accent }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Albums")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 23
                    font.weight: Font.Bold
                }
                Text {
                    text: qsTr("%1 album(s)").arg(root.lib.albumsModel.count)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }
        }

        GridView {
            id: albumGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            readonly property int columns: Math.max(2, Math.floor((Math.max(360, width) - 8) / 188))
            cellWidth: Math.floor((Math.max(360, width) - 8) / columns)
            cellHeight: cellWidth + 56
            cacheBuffer: 360
            boundsBehavior: Flickable.StopAtBounds
            model: root.lib.albumsModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: card
                required property string album
                required property string albumArtist
                required property int year
                required property int trackCount
                required property string coverHash
                required property string formatBadge

                width: albumGrid.cellWidth
                height: albumGrid.cellHeight
                readonly property bool hovered: cardMa.containsMouse || quickPlayMa.containsMouse

                Rectangle {
                    id: cardSurface
                    x: 7
                    y: card.hovered ? 3 : 7
                    width: parent.width - 14
                    height: parent.height - 14
                    radius: DesignTokens.radiusLg
                    color: card.hovered ? DesignTokens.surfaceCardHover : DesignTokens.surfaceCard
                    border.width: 1
                    border.color: card.hovered ? DesignTokens.borderHighlight : DesignTokens.borderSubtle

                    Behavior on y { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 7

                        Rectangle {
                            id: coverBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            radius: DesignTokens.radiusMd
                            color: DesignTokens.surfaceInput
                            border.width: 1
                            border.color: DesignTokens.borderSubtle
                            clip: true

                            Rectangle {
                                id: gridCoverMask
                                anchors.fill: parent
                                radius: DesignTokens.radiusMd
                                visible: false
                                layer.enabled: true
                                layer.smooth: true
                            }

                            Image {
                                id: gridCoverImg
                                anchors.fill: parent
                                source: card.coverHash.length > 0 ? ("image://cover/" + card.coverHash + "?size=280") : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: false
                            }

                            MultiEffect {
                                anchors.fill: gridCoverImg
                                visible: gridCoverImg.status === Image.Ready
                                source: gridCoverImg
                                maskEnabled: true
                                maskSource: gridCoverMask
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                                scale: card.hovered ? 1.04 : 1.0
                                Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                            }

                            IconGlyph {
                                anchors.centerIn: parent
                                name: "album"
                                size: 42
                                color: DesignTokens.textTertiary
                                visible: gridCoverImg.status !== Image.Ready
                            }

                            FormatBadge {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.margins: 7
                                compact: true
                                text: card.formatBadge
                            }

                            // Hover Quick-Play Accent Circle
                            Item {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 8
                                width: 36
                                height: 36
                                z: 5
                                opacity: card.hovered ? 1.0 : 0.0
                                scale: quickPlayMa.pressed
                                    ? 0.90
                                    : (quickPlayMa.containsMouse ? 1.10 : (card.hovered ? 1.0 : 0.72))

                                Behavior on opacity { NumberAnimation { duration: 150 } }
                                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 1.15 } }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: 21
                                    color: Qt.rgba(0, 0, 0, 0.32)
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 18
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: DesignTokens.accentLight }
                                        GradientStop { position: 1.0; color: DesignTokens.accent }
                                    }

                                    IconGlyph {
                                        anchors.centerIn: parent
                                        anchors.horizontalCenterOffset: 1
                                        name: "play"
                                        size: 15
                                        color: "#ffffff"
                                    }
                                }

                                MouseArea {
                                    id: quickPlayMa
                                    anchors.fill: parent
                                    enabled: card.hovered
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.lib.openAlbum(card.album, card.albumArtist)
                                        root.lib.playFromDetailModel(0)
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.album
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.year > 0 ? (card.albumArtist + " · " + card.year) : card.albumArtist
                            elide: Text.ElideRight
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        id: cardMa
                        anchors.fill: parent
                        z: -1
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedAlbum = card.album
                            root.selectedArtist = card.albumArtist
                            root.selectedCoverHash = card.coverHash
                            root.selectedYear = card.year
                            root.lib.openAlbum(card.album, card.albumArtist)
                            root.inDetail = true
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                visible: root.lib.albumsModel.count === 0
                spacing: 8
                IconGlyph {
                    Layout.alignment: Qt.AlignHCenter
                    name: "album"; size: 36; color: DesignTokens.textTertiary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No albums indexed yet")
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 14
                }
            }
        }
    }

    // Album Detail View
    ColumnLayout {
        id: detailPane
        visible: root.inDetail
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14
        transform: Translate { id: detailShift; x: 0 }

        RowLayout {
            spacing: 8
            SonoraButton {
                compact: true
                iconName: "chevron_left"
                text: qsTr("Albums")
                onClicked: root.inDetail = false
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: 172
            radius: DesignTokens.radiusLg

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 20

                Rectangle {
                    implicitWidth: 144
                    implicitHeight: 144
                    radius: DesignTokens.radiusMd
                    color: DesignTokens.surfaceInput
                    border.width: 1
                    border.color: DesignTokens.borderHighlight
                    clip: true

                    Rectangle {
                        id: detailCoverMask
                        anchors.fill: parent
                        radius: DesignTokens.radiusMd
                        visible: false
                        layer.enabled: true
                        layer.smooth: true
                    }

                    Image {
                        id: detailCoverImg
                        anchors.fill: parent
                        source: root.selectedCoverHash.length > 0 ? ("image://cover/" + root.selectedCoverHash + "?size=360") : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                        visible: false
                    }

                    MultiEffect {
                        anchors.fill: detailCoverImg
                        visible: detailCoverImg.status === Image.Ready
                        source: detailCoverImg
                        maskEnabled: true
                        maskSource: detailCoverMask
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }
                    IconGlyph {
                        anchors.centerIn: parent
                        name: "album"
                        size: 46
                        color: DesignTokens.textTertiary
                        visible: detailCoverImg.status !== Image.Ready
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        Layout.fillWidth: true
                        text: root.selectedAlbum
                        elide: Text.ElideRight
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: root.selectedYear > 0 ? (root.selectedArtist + " · " + root.selectedYear) : root.selectedArtist
                        color: DesignTokens.accent
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("%1 track(s)").arg(root.lib.detailTracksModel.count)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    RowLayout {
                        Layout.topMargin: 6
                        spacing: 10
                        SonoraButton {
                            primary: true
                            iconName: "play"
                            text: qsTr("Play Album")
                            onClicked: root.lib.playFromDetailModel(0)
                        }
                    }
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
                id: dtRow
                required property int index
                required property int trackId
                required property int trackNumber
                required property string title
                required property string artist
                required property int durationMs
                required property string formatBadge

                readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                width: ListView.view ? ListView.view.width : 600
                height: 44
                radius: 10
                color: isCurrent
                    ? DesignTokens.surfaceSelected
                    : (dtMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                border.width: isCurrent ? 1 : 0
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                Behavior on color { ColorAnimation { duration: 110 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 14
                    spacing: 12

                    Item {
                        Layout.preferredWidth: 28
                        implicitHeight: 20

                        NowPlayingBars {
                            anchors.centerIn: parent
                            visible: dtRow.isCurrent
                            playing: root.controller.isPlaying
                            size: 13
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !dtRow.isCurrent
                            text: dtRow.trackNumber > 0 ? String(dtRow.trackNumber) : String(dtRow.index + 1)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: dtRow.title
                        elide: Text.ElideRight
                        color: dtRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: dtRow.isCurrent ? Font.DemiBold : Font.Medium
                    }

                    FormatBadge { compact: true; text: dtRow.formatBadge }

                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(dtRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.features: ({ "tnum": 1 })
                    }
                }

                MouseArea {
                    id: dtMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromDetailModel(dtRow.index)
                }
            }
        }
    }
}
