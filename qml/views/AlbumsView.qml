import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
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

    // Album Grid View
    ColumnLayout {
        visible: !root.inDetail
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Albums")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 24
                font.weight: Font.Bold
            }
            Text {
                text: qsTr("%1 album(s)").arg(root.lib.albumsModel.count)
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 12
            }
        }

        GridView {
            id: albumGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: 186
            cellHeight: 238
            cacheBuffer: 320
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

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 7
                    radius: DesignTokens.radiusLg
                    color: cardMa.containsMouse ? DesignTokens.surfaceCard : "transparent"
                    border.width: cardMa.containsMouse ? 1 : 0
                    border.color: DesignTokens.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 7

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 156
                            radius: DesignTokens.radiusMd
                            color: DesignTokens.surfaceCard
                            border.width: 1
                            border.color: DesignTokens.borderSubtle
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: card.coverHash.length > 0 ? ("image://cover/" + card.coverHash + "?size=200") : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: status === Image.Ready
                            }

                            IconGlyph {
                                anchors.centerIn: parent
                                name: "album"
                                size: 42
                                color: DesignTokens.textTertiary
                                visible: card.coverHash.length === 0
                            }

                            FormatBadge {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                compact: true
                                text: card.formatBadge
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

            Text {
                anchors.centerIn: parent
                visible: root.lib.albumsModel.count === 0
                text: qsTr("No albums indexed yet")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 14
            }
        }
    }

    // Album Detail View
    ColumnLayout {
        visible: root.inDetail
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        RowLayout {
            spacing: 8
            Rectangle {
                implicitHeight: 30; implicitWidth: backRow.implicitWidth + 18; radius: 15
                color: backMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                RowLayout {
                    id: backRow
                    anchors.centerIn: parent; spacing: 4
                    IconGlyph { name: "chevron_left"; size: 14; color: DesignTokens.textPrimary }
                    Text {
                        text: qsTr("Albums")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    id: backMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.inDetail = false
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Rectangle {
                implicitWidth: 152; implicitHeight: 152; radius: DesignTokens.radiusLg
                color: DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                clip: true

                Image {
                    anchors.fill: parent
                    source: root.selectedCoverHash.length > 0 ? ("image://cover/" + root.selectedCoverHash + "?size=360") : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: status === Image.Ready
                }
                IconGlyph {
                    anchors.centerIn: parent; name: "album"; size: 46; color: DesignTokens.textTertiary
                    visible: root.selectedCoverHash.length === 0
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: root.selectedAlbum
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }
                Text {
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
                Rectangle {
                    Layout.topMargin: 6
                    implicitHeight: 34; implicitWidth: playAlbRow.implicitWidth + 26; radius: 17
                    color: playAlbMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent
                    RowLayout {
                        id: playAlbRow; anchors.centerIn: parent; spacing: 6
                        IconGlyph { name: "play"; size: 13; color: "#ffffff" }
                        Text {
                            text: qsTr("Play Album")
                            color: "#ffffff"
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }
                    MouseArea {
                        id: playAlbMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.playFromDetailModel(0)
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            reuseItems: true
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
                height: 42
                radius: 8
                color: isCurrent ? DesignTokens.surfaceSelected
                                 : (dtMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 12

                    Text {
                        Layout.preferredWidth: 28
                        text: dtRow.trackNumber > 0 ? String(dtRow.trackNumber) : String(dtRow.index + 1)
                        color: dtRow.isCurrent ? DesignTokens.accent : DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                    Text {
                        Layout.fillWidth: true
                        text: dtRow.title
                        elide: Text.ElideRight
                        color: dtRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: dtRow.isCurrent ? Font.DemiBold : Font.Normal
                    }
                    FormatBadge { compact: true; text: dtRow.formatBadge }
                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(dtRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    id: dtMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromDetailModel(dtRow.index)
                }
            }
        }
    }
}
