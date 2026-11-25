import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib
    property string sectionTitle: qsTr("All Tracks")
    property string sectionKey: "tracks"
    property int activePlaylistId: -1

    signal addFolderRequested()
    signal openFilesRequested()
    signal exportPlaylistRequested(int playlistId)
    signal deletePlaylistRequested(int playlistId)
    signal inspectTrack(var trackMap)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // Section Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: root.sectionTitle
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }

                Text {
                    text: root.lib.scanning
                        ? qsTr("Scanning library... (%1 / %2)").arg(root.lib.scanProgress).arg(root.lib.scanTotal)
                        : qsTr("%1 track(s)").arg(root.lib.tracksModel.count)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }

            // Play All primary pill
            Rectangle {
                visible: root.lib.tracksModel.count > 0
                implicitHeight: 34
                implicitWidth: playAllRow.implicitWidth + 24
                radius: 17
                color: playAllMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent

                RowLayout {
                    id: playAllRow
                    anchors.centerIn: parent
                    spacing: 6
                    IconGlyph { name: "play"; size: 13; color: "#ffffff" }
                    Text {
                        text: qsTr("Play All")
                        color: "#ffffff"
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }
                MouseArea {
                    id: playAllMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromTracksModel(0)
                }
            }

            // Export M3U8 (playlist view only)
            Rectangle {
                visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
                implicitHeight: 34
                implicitWidth: expRow.implicitWidth + 20
                radius: 17
                color: expMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle

                RowLayout {
                    id: expRow
                    anchors.centerIn: parent
                    spacing: 6
                    IconGlyph { name: "export"; size: 13; color: DesignTokens.textSecondary }
                    Text {
                        text: qsTr("Export M3U8")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    id: expMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.exportPlaylistRequested(root.activePlaylistId)
                }
            }

            // Delete Playlist
            Rectangle {
                visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
                implicitWidth: 34; implicitHeight: 34; radius: 17
                color: delPlMa.containsMouse ? Qt.rgba(0.85, 0.22, 0.28, 0.20) : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle

                IconGlyph { anchors.centerIn: parent; name: "trash"; size: 14; color: DesignTokens.textSecondary }
                MouseArea {
                    id: delPlMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.deletePlaylistRequested(root.activePlaylistId)
                }
            }
        }

        // Column Sort Header Bar
        Rectangle {
            visible: root.lib.tracksModel.count > 0
            Layout.fillWidth: true
            implicitHeight: 30
            radius: 8
            color: DesignTokens.surfaceCard
            border.width: 1
            border.color: DesignTokens.borderSubtle

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: "#"
                    Layout.preferredWidth: 28
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                }

                Text {
                    text: qsTr("Title") + (root.lib.sortBy === "title" ? (root.lib.sortAscending ? " ↑" : " ↓") : "")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 240
                    color: root.lib.sortBy === "title" ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.sortBy = "title"
                    }
                }

                Text {
                    text: qsTr("Artist") + (root.lib.sortBy === "artist" ? (root.lib.sortAscending ? " ↑" : " ↓") : "")
                    Layout.preferredWidth: 160
                    color: root.lib.sortBy === "artist" ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.sortBy = "artist"
                    }
                }

                Text {
                    text: qsTr("Album") + (root.lib.sortBy === "album" ? (root.lib.sortAscending ? " ↑" : " ↓") : "")
                    Layout.preferredWidth: 160
                    color: root.lib.sortBy === "album" ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.sortBy = "album"
                    }
                }

                Text {
                    text: qsTr("Format")
                    Layout.preferredWidth: 74
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Time") + (root.lib.sortBy === "duration" ? (root.lib.sortAscending ? " ↑" : " ↓") : "")
                    Layout.preferredWidth: 52
                    horizontalAlignment: Text.AlignRight
                    color: root.lib.sortBy === "duration" ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.sortBy = "duration"
                    }
                }

                Item { Layout.preferredWidth: 64 }
            }
        }

        // Virtualized Track List (reuseItems: true for minimal RAM on 50k+ libraries)
        ListView {
            id: trackList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            reuseItems: true
            cacheBuffer: 240
            model: root.lib.tracksModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: row
                required property int index
                required property int trackId
                required property string title
                required property string artist
                required property string album
                required property int durationMs
                required property string formatBadge
                required property string coverHash
                required property bool isFavorite
                required property bool isCue

                readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                width: trackList.width
                height: 48
                radius: 9
                color: isCurrent
                    ? DesignTokens.surfaceSelected
                    : (rowMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12

                    // Track index or Cover thumbnail
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 6
                        color: DesignTokens.surfaceCard
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: row.coverHash.length > 0 ? ("image://cover/" + row.coverHash + "?size=96") : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            visible: status === Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: row.coverHash.length === 0
                            text: String(row.index + 1)
                            color: row.isCurrent ? DesignTokens.accent : DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }

                    // Title + CUE indicator
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 220
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: row.title
                            elide: Text.ElideRight
                            color: row.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: row.isCurrent ? Font.DemiBold : Font.Normal
                        }

                        FormatBadge {
                            compact: true
                            visible: row.isCue
                            text: "CUE"
                        }
                    }

                    Text {
                        Layout.preferredWidth: 160
                        text: row.artist.length > 0 ? row.artist : qsTr("Unknown Artist")
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.preferredWidth: 160
                        text: row.album
                        elide: Text.ElideRight
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    Item {
                        Layout.preferredWidth: 74
                        implicitHeight: 22
                        FormatBadge {
                            anchors.verticalCenter: parent.verticalCenter
                            compact: true
                            text: row.formatBadge
                        }
                    }

                    Text {
                        Layout.preferredWidth: 52
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(row.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    // Quick inline action buttons (Favorite, Play Next / Menu, Info)
                    RowLayout {
                        Layout.preferredWidth: 64
                        spacing: 4

                        Rectangle {
                            implicitWidth: 26; implicitHeight: 26; radius: 6
                            color: favMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            IconGlyph {
                                anchors.centerIn: parent
                                name: row.isFavorite ? "heart_filled" : "heart"
                                size: 13
                                color: row.isFavorite ? DesignTokens.accent : DesignTokens.textTertiary
                            }
                            MouseArea {
                                id: favMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.lib.toggleFavorite(row.trackId, row.isFavorite)
                            }
                        }

                        Rectangle {
                            implicitWidth: 26; implicitHeight: 26; radius: 6
                            color: infoMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            IconGlyph {
                                anchors.centerIn: parent
                                name: "info"
                                size: 13
                                color: DesignTokens.textTertiary
                            }
                            MouseArea {
                                id: infoMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.inspectTrack(root.lib.tracksModel.trackMapAt(row.index))
                            }
                        }
                    }
                }

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    anchors.rightMargin: 76
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: root.lib.playFromTracksModel(row.index)
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            trackMenu.targetIndex = row.index
                            trackMenu.targetTrackId = row.trackId
                            trackMenu.popup()
                        }
                    }
                }
            }

            // Empty Library Onboarding Card
            ColumnLayout {
                anchors.centerIn: parent
                visible: root.lib.tracksModel.count === 0
                spacing: 12
                width: Math.min(parent.width - 80, 440)

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 64; implicitHeight: 64; radius: 18
                    color: DesignTokens.surfaceCard
                    border.width: 1; border.color: DesignTokens.borderSubtle
                    IconGlyph { anchors.centerIn: parent; name: "music"; size: 28; color: DesignTokens.accent }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.sectionKey === "tracks" ? qsTr("Your Library is Empty") : qsTr("No Tracks Here Yet")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Add your local music folders to index FLAC, ALAC, WAV, APE, CUE, DSD, and MP3 files, or drag files straight onto this window.")
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 13
                    lineHeight: 1.5
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    spacing: 10

                    Rectangle {
                        implicitHeight: 36
                        implicitWidth: addFolderLbl.implicitWidth + 28
                        radius: 18
                        color: addFolderMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent
                        Text {
                            id: addFolderLbl
                            anchors.centerIn: parent
                            text: qsTr("Add Music Folder")
                            color: "#ffffff"
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            id: addFolderMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.addFolderRequested()
                        }
                    }

                    Rectangle {
                        implicitHeight: 36
                        implicitWidth: quickPlayLbl.implicitWidth + 28
                        radius: 18
                        color: quickPlayMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                        border.width: 1; border.color: DesignTokens.borderSubtle
                        Text {
                            id: quickPlayLbl
                            anchors.centerIn: parent
                            text: qsTr("Open Audio Files")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                        }
                        MouseArea {
                            id: quickPlayMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openFilesRequested()
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: trackMenu
        property int targetIndex: -1
        property int targetTrackId: -1

        MenuItem {
            text: qsTr("Play Now")
            onTriggered: root.lib.playFromTracksModel(trackMenu.targetIndex)
        }
        MenuItem {
            text: qsTr("Play Next")
            onTriggered: root.lib.playNextFromTracksModel(trackMenu.targetIndex)
        }
        MenuItem {
            text: qsTr("Add to Queue")
            onTriggered: root.lib.enqueueFromTracksModel(trackMenu.targetIndex)
        }
        Menu {
            title: qsTr("Add to Playlist")
            enabled: root.lib.playlists.length > 0
            Repeater {
                model: root.lib.playlists
                MenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: root.lib.addTrackToPlaylist(modelData.id, trackMenu.targetTrackId)
                }
            }
        }
        MenuItem {
            visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
            text: qsTr("Remove from Playlist")
            onTriggered: root.lib.removeTrackFromPlaylist(root.activePlaylistId, trackMenu.targetIndex)
        }
        MenuItem {
            text: qsTr("Track Technical Info")
            onTriggered: root.inspectTrack(root.lib.tracksModel.trackMapAt(trackMenu.targetIndex))
        }
    }
}
