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

    readonly property bool compact: width < 650
    readonly property bool shortWindow: height < 540
    readonly property int metadataColumnWidth: Math.min(132, Math.max(90, Math.round(width * 0.15)))
    readonly property int actionColumnWidth: compact ? 0 : 54

    readonly property string sectionIcon: {
        if (sectionKey === "favorites") return "heart_filled"
        if (sectionKey === "recent") return "clock"
        if (sectionKey === "playlist") return "playlist"
        return "music"
    }

    component SortHeaderCell: Item {
        id: cell
        property string label: ""
        property string sortKey: ""
        property int hAlign: Text.AlignLeft
        readonly property bool isSorted: sortKey.length > 0 && root.lib.sortBy === sortKey

        implicitHeight: 28

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: -6
            anchors.rightMargin: -6
            radius: 6
            color: cellMa.containsMouse && cell.sortKey.length > 0 ? DesignTokens.surfaceHover : "transparent"
            Behavior on color { ColorAnimation { duration: 110 } }
        }

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: cell.hAlign
            elide: Text.ElideRight
            text: cell.label + (cell.isSorted ? (root.lib.sortAscending ? " ↑" : " ↓") : "")
            color: cell.isSorted
                ? DesignTokens.accent
                : (cellMa.containsMouse && cell.sortKey.length > 0 ? DesignTokens.textSecondary : DesignTokens.textTertiary)
            font.family: DesignTokens.fontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: cellMa
            anchors.fill: parent
            enabled: cell.sortKey.length > 0
            hoverEnabled: cell.sortKey.length > 0
            cursorShape: cell.sortKey.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (cell.sortKey.length > 0) root.lib.sortBy = cell.sortKey
        }
    }

    component StyledMenuItem: MenuItem {
        id: mi
        implicitHeight: 32
        implicitWidth: 196

        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            text: mi.text
            color: mi.highlighted ? DesignTokens.accent : DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
            font.weight: mi.highlighted ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: DesignTokens.radiusSm
            color: mi.highlighted ? DesignTokens.surfaceSelected : "transparent"
        }
    }

    Rectangle {
        x: libraryLayout.x + sortHeader.x
        y: libraryLayout.y + sortHeader.y
        width: sortHeader.width
        height: libraryLayout.height - sortHeader.y
        visible: sortHeader.visible
        radius: DesignTokens.radiusLg
        color: DesignTokens.contentStageBg
        border.width: 1
        border.color: DesignTokens.hairlineBorder
    }

    ColumnLayout {
        id: libraryLayout
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 6 : 0
        anchors.rightMargin: root.compact ? 6 : 0
        spacing: root.shortWindow ? 8 : 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                    text: root.sectionTitle
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    font.letterSpacing: -0.3
                }

                Rectangle {
                    Layout.maximumWidth: 260
                    implicitWidth: Math.min(260, trackCountText.implicitWidth + 18)
                    implicitHeight: 24
                    radius: 12
                    color: root.lib.scanning ? DesignTokens.accentSoft : DesignTokens.surfaceInput
                    border.width: 1
                    border.color: root.lib.scanning
                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.30)
                        : DesignTokens.hairlineBorder

                    Text {
                        id: trackCountText
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width - 14)
                        elide: Text.ElideRight
                        text: root.lib.scanning
                            ? qsTr("Scanning library... (%1 / %2)").arg(root.lib.scanProgress).arg(root.lib.scanTotal)
                            : qsTr("%1 track(s)").arg(root.lib.tracksModel.count)
                        color: root.lib.scanning ? DesignTokens.accent : DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.weight: root.lib.scanning ? Font.DemiBold : Font.Medium
                        font.features: ({ "tnum": 1 })
                    }
                }

                Item { Layout.fillWidth: true }
            }

            SonoraButton {
                visible: root.lib.tracksModel.count > 0
                compact: true
                iconName: "play"
                text: qsTr("Play All")
                onClicked: root.lib.playFromTracksModel(0)
            }

            SonoraButton {
                visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
                compact: true
                iconName: "export"
                text: qsTr("Export M3U8")
                onClicked: root.exportPlaylistRequested(root.activePlaylistId)
            }

            Rectangle {
                visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
                implicitWidth: 34
                implicitHeight: 34
                radius: 17
                color: delPlMa.containsMouse ? Qt.rgba(0.88, 0.22, 0.30, 0.20) : DesignTokens.surfaceCard
                border.width: 1
                border.color: delPlMa.containsMouse ? Qt.rgba(0.88, 0.22, 0.30, 0.45) : DesignTokens.borderOuter
                scale: delPlMa.pressed ? 0.92 : (delPlMa.containsMouse ? 1.05 : 1.0)
                Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }
                Behavior on color { ColorAnimation { duration: 130 } }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "trash"
                    size: 14
                    color: delPlMa.containsMouse ? "#f25565" : DesignTokens.textSecondary
                }
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
        Item {
            id: sortHeader
            visible: root.lib.tracksModel.count > 0
            Layout.fillWidth: true
            Layout.bottomMargin: -6
            implicitHeight: root.compact ? 32 : 38

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                anchors.bottomMargin: 0
                height: 1
                color: DesignTokens.hairlineBorder
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 17
                anchors.rightMargin: 17
                spacing: 9

                Text {
                    text: "#"
                    Layout.preferredWidth: 27
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                SortHeaderCell {
                    label: qsTr("Title")
                    sortKey: "title"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 72
                }

                SortHeaderCell {
                    visible: !root.compact
                    label: qsTr("Artist")
                    sortKey: "artist"
                    Layout.preferredWidth: root.metadataColumnWidth
                }

                SortHeaderCell {
                    visible: !root.compact
                    label: qsTr("Album")
                    sortKey: "album"
                    Layout.preferredWidth: root.metadataColumnWidth
                }

                SortHeaderCell {
                    label: qsTr("Format")
                    Layout.preferredWidth: root.compact ? 64 : 68
                }

                SortHeaderCell {
                    label: qsTr("Time")
                    sortKey: "duration"
                    hAlign: Text.AlignRight
                    Layout.preferredWidth: 44
                }

                Item { visible: !root.compact; Layout.preferredWidth: root.actionColumnWidth }
            }
        }

        // Virtualized Track List (reuseItems: true for minimal RAM on 50k+ libraries)
        ListView {
            id: trackList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: root.shortWindow && root.compact ? 14 : 0
            clip: true
            spacing: 1
            reuseItems: true
            cacheBuffer: 240
            boundsBehavior: Flickable.StopAtBounds
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
                readonly property bool rowHovered: rowMa.containsMouse || thumbMa.containsMouse || favMa.containsMouse || infoMa.containsMouse
                width: trackList.width
                height: root.shortWindow ? 38 : 44
                radius: DesignTokens.radiusSm
                color: isCurrent
                    ? DesignTokens.surfaceSelected
                    : (rowHovered ? DesignTokens.surfaceHover : "transparent")
                border.width: isCurrent ? 1 : 0
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.28 : 0.18)
                Behavior on color { ColorAnimation { duration: 110 } }

                Rectangle {
                    visible: row.isCurrent
                    width: 3
                    height: 18
                    radius: 1.5
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: DesignTokens.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    spacing: 9

                    Item {
                        Layout.preferredWidth: 27
                        implicitHeight: 18

                        NowPlayingBars {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.isCurrent
                            playing: root.controller.isPlaying
                            size: 14
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !row.isCurrent
                            text: String(row.index + 1).padStart(2, "0")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    // Cover thumbnail and hover transport.
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 8
                        color: DesignTokens.surfaceCard
                        border.width: 1
                        border.color: row.isCurrent ? DesignTokens.accent : DesignTokens.borderSubtle
                        clip: true

                        Image {
                            id: thumbImg
                            anchors.fill: parent
                            source: row.isCurrent ? root.controller.trackCoverUrl
                                : (row.coverHash.length > 0 ? "image://cover/" + row.coverHash + "?size=96" : "")
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            visible: status === Image.Ready
                        }

                        IconGlyph {
                            anchors.centerIn: parent
                            visible: thumbImg.status !== Image.Ready && !thumbMa.containsMouse
                            name: "music"
                            size: 15
                            color: DesignTokens.textTertiary
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: thumbMa.containsMouse
                            color: thumbImg.visible ? Qt.rgba(0, 0, 0, 0.46) : "transparent"

                            IconGlyph {
                                anchors.centerIn: parent
                                name: row.isCurrent && root.controller.isPlaying ? "pause" : "play"
                                size: 14
                                color: thumbImg.visible ? "#ffffff" : DesignTokens.accent
                            }
                        }

                        MouseArea {
                            id: thumbMa
                            anchors.fill: parent
                            z: 5
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (row.isCurrent) root.controller.togglePlayPause()
                                else root.lib.playFromTracksModel(row.index)
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: row.title
                                elide: Text.ElideRight
                                color: row.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 14
                                font.weight: row.isCurrent ? Font.DemiBold : Font.Medium
                            }

                            FormatBadge {
                                compact: true
                                visible: row.isCue
                                text: "CUE"
                            }
                        }

                        Text {
                            visible: root.compact
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: row.artist.length > 0 ? row.artist : qsTr("Unknown Artist")
                            elide: Text.ElideRight
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }

                    Text {
                        visible: !root.compact
                        Layout.preferredWidth: root.metadataColumnWidth
                        text: row.artist.length > 0 ? row.artist : qsTr("Unknown Artist")
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    Text {
                        visible: !root.compact
                        Layout.preferredWidth: root.metadataColumnWidth
                        text: row.album
                        elide: Text.ElideRight
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    Item {
                        Layout.preferredWidth: root.compact ? 64 : 68
                        implicitHeight: 22
                        FormatBadge {
                            anchors.verticalCenter: parent.verticalCenter
                            compact: true
                            text: row.formatBadge
                        }
                    }

                    Text {
                        Layout.preferredWidth: 44
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(row.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.features: ({ "tnum": 1 })
                    }

                    // Quick inline action buttons (Favorite, Info)
                    RowLayout {
                        visible: !root.compact
                        Layout.preferredWidth: root.actionColumnWidth
                        spacing: 2

                        Rectangle {
                            implicitWidth: 26
                            implicitHeight: 26
                            radius: 8
                            color: favMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            scale: favMa.pressed ? 0.88 : (favMa.containsMouse ? 1.10 : 1.0)
                            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }

                            IconGlyph {
                                anchors.centerIn: parent
                                name: row.isFavorite ? "heart_filled" : "heart"
                                size: 13
                                color: row.isFavorite
                                    ? DesignTokens.accent
                                    : (row.rowHovered ? DesignTokens.textSecondary : DesignTokens.textTertiary)
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
                            implicitWidth: 26
                            implicitHeight: 26
                            radius: 8
                            color: infoMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            opacity: row.rowHovered ? 1.0 : 0.55
                            Behavior on opacity { NumberAnimation { duration: 120 } }

                            IconGlyph {
                                anchors.centerIn: parent
                                name: "info"
                                size: 13
                                color: infoMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textTertiary
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
                    anchors.leftMargin: 86
                    anchors.rightMargin: root.actionColumnWidth + 8
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: root.lib.playFromTracksModel(row.index)
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            trackMenu.targetIndex = row.index
                            trackMenu.targetTrackId = row.trackId
                            trackMenu.targetIsFavorite = row.isFavorite
                            trackMenu.popup()
                        }
                    }
                }
            }

            // Spatial Glass Empty Library Onboarding Card
            GlassPanel {
                anchors.centerIn: parent
                visible: root.lib.tracksModel.count === 0
                width: Math.min(parent.width - 64, 460)
                height: emptyCol.implicitHeight + 52
                radius: DesignTokens.radiusXl
                panelColor: DesignTokens.stageSurfaceBg
                borderColor: DesignTokens.hairlineBorder
                shadowEnabled: false

                ColumnLayout {
                    id: emptyCol
                    anchors.centerIn: parent
                    width: parent.width - 56
                    spacing: 12

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 64
                        implicitHeight: 64
                        radius: 20
                        color: DesignTokens.accentSoft
                        border.width: 1
                        border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.28)
                        IconGlyph { anchors.centerIn: parent; name: root.sectionIcon; size: 28; color: DesignTokens.accent }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.sectionKey === "tracks" ? qsTr("Your Library is Empty") : qsTr("No Tracks Here Yet")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 19
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Add your local music folders to index FLAC, ALAC, WAV, APE, CUE, DSD, and MP3 files, or drag files straight onto this window.")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        lineHeight: 1.45
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 8
                        spacing: 10

                        SonoraButton {
                            primary: true
                            iconName: "folder"
                            text: qsTr("Add Music Folder")
                            onClicked: root.addFolderRequested()
                        }

                        SonoraButton {
                            iconName: "plus"
                            text: qsTr("Open Audio Files")
                            onClicked: root.openFilesRequested()
                        }
                    }
                }
            }
        }
    }

    // Spatial Glass Context Menu
    Menu {
        id: trackMenu
        property int targetIndex: -1
        property int targetTrackId: -1
        property bool targetIsFavorite: false
        padding: 6

        background: GlassPanel {
            implicitWidth: 208
            radius: DesignTokens.radiusMd
            elevated: true
            shadowEnabled: true
        }

        StyledMenuItem {
            text: qsTr("Play Now")
            onTriggered: root.lib.playFromTracksModel(trackMenu.targetIndex)
        }
        StyledMenuItem {
            text: qsTr("Play Next")
            onTriggered: root.lib.playNextFromTracksModel(trackMenu.targetIndex)
        }
        StyledMenuItem {
            text: qsTr("Add to Queue")
            onTriggered: root.lib.enqueueFromTracksModel(trackMenu.targetIndex)
        }

        Menu {
            id: playlistSubMenu
            title: qsTr("Add to Playlist")
            enabled: root.lib.playlists.length > 0
            padding: 6

            background: GlassPanel {
                implicitWidth: 190
                radius: DesignTokens.radiusMd
                elevated: true
                shadowEnabled: true
            }

            Repeater {
                model: root.lib.playlists
                StyledMenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: root.lib.addTrackToPlaylist(modelData.id, trackMenu.targetTrackId)
                }
            }
        }

        StyledMenuItem {
            visible: root.sectionKey === "playlist" && root.activePlaylistId >= 0
            text: qsTr("Remove from Playlist")
            onTriggered: root.lib.removeTrackFromPlaylist(root.activePlaylistId, trackMenu.targetIndex)
        }
        StyledMenuItem {
            text: trackMenu.targetIsFavorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            onTriggered: root.lib.toggleFavorite(trackMenu.targetTrackId, trackMenu.targetIsFavorite)
        }
        StyledMenuItem {
            text: qsTr("Track Technical Info")
            onTriggered: root.inspectTrack(root.lib.tracksModel.trackMapAt(trackMenu.targetIndex))
        }
    }
}
