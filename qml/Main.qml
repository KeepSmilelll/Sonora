import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Sonora.UI

Window {
    id: win

    width: 1200
    height: 780
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Sonora"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: DesignTokens.nativeBackdropActive ? "transparent" : DesignTokens.windowBg

    property string activeSection: "tracks"
    property int activePlaylistId: -1
    property string activePlaylistName: ""
    property bool queueOpen: false
    property bool lyricsOpen: false
    property bool eqOpen: false
    property bool miniModeOpen: false
    property bool createPlaylistOpen: false
    property var inspectedTrackMap: null
    property int exportPlaylistTargetId: -1

    function syncThemeTokens() {
        DesignTokens.isDark = themeManager.isDark
        DesignTokens.nativeBackdropActive = themeManager.nativeBackdropActive
        DesignTokens.backdropMode = themeManager.backdropMode
        DesignTokens.ambientPrimary = themeManager.ambientPrimary
        DesignTokens.ambientSecondary = themeManager.ambientSecondary
        DesignTokens.accent = themeManager.accentColor
    }

    function toggleMiniMode() {
        miniModeOpen = !miniModeOpen
        if (miniModeOpen) {
            win.hide()
        } else {
            win.showNormal()
            win.raise()
            win.requestActivate()
        }
    }

    Component.onCompleted: syncThemeTokens()

    onClosing: (close) => {
        if (Qt.platform.os === "windows" && appSettings.closeToTray && !miniModeOpen) {
            close.accepted = false
            win.hide()
        }
    }

    Connections {
        target: themeManager
        function onThemeChanged() { win.syncThemeTokens() }
        function onBackdropChanged() { win.syncThemeTokens() }
        function onAmbientColorsChanged() { win.syncThemeTokens() }
    }

    Connections {
        target: platformBridge
        function onToggleMiniModeRequested() { win.toggleMiniMode() }
        function onShowMainWindowRequested() {
            win.miniModeOpen = false
            win.showNormal()
            win.raise()
            win.requestActivate()
        }
    }

    AmbientBackdrop {
        anchors.fill: parent
    }

    // Native OS Dialogs (zero Qt6Widgets)
    FileDialog {
        id: openAudioDialog
        title: qsTr("Open Audio Files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Audio & Playlists (*.flac *.wav *.aiff *.alac *.ape *.wv *.tta *.mp3 *.m4a *.aac *.ogg *.opus *.wma *.dsf *.dff *.cue *.m3u *.m3u8)"),
            qsTr("All Files (*)")
        ]
        onAccepted: player.openUrls(selectedFiles, false)
    }

    FolderDialog {
        id: addFolderDialog
        title: qsTr("Select Music Folder")
        onAccepted: library.addWatchedFolder(selectedFolder)
    }

    FileDialog {
        id: importM3uDialog
        title: qsTr("Import M3U / M3U8 Playlist")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("M3U Playlists (*.m3u *.m3u8)"), qsTr("All Files (*)")]
        onAccepted: library.importM3uPlaylist(selectedFile)
    }

    FileDialog {
        id: exportM3uDialog
        title: qsTr("Export M3U8 Playlist")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "m3u8"
        nameFilters: [qsTr("M3U8 Playlist (*.m3u8)")]
        onAccepted: {
            if (win.exportPlaylistTargetId >= 0) {
                library.exportM3uPlaylist(win.exportPlaylistTargetId, selectedFile)
            }
        }
    }

    // Main Application Frame
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TitleBar {
            Layout.fillWidth: true
            searchText: library.searchQuery
            isMaximized: win.visibility === Window.Maximized
            onSearchEdited: (text) => library.searchQuery = text
            onOpenFilesRequested: openAudioDialog.open()
            onToggleTheme: themeManager.toggleDarkLight()
            onToggleMiniMode: win.toggleMiniMode()
            onMinimizeClicked: win.showMinimized()
            onMaximizeRestoreClicked: {
                if (win.visibility === Window.Maximized) {
                    win.showNormal()
                } else {
                    win.showMaximized()
                }
            }
            onCloseClicked: win.close()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.fillHeight: true
                activeSection: win.activeSection
                customPlaylists: library.playlists
                activePlaylistId: win.activePlaylistId
                onSectionSelected: (section) => {
                    win.activeSection = section
                    win.activePlaylistId = -1
                    if (section === "tracks" || section === "favorites" || section === "recent" || section === "folders") {
                        library.loadSection(section)
                    }
                }
                onPlaylistSelected: (plId, plName) => {
                    win.activeSection = "playlist"
                    win.activePlaylistId = plId
                    win.activePlaylistName = plName
                    library.openPlaylist(plId)
                }
                onCreatePlaylistRequested: win.createPlaylistOpen = true
                onImportM3uRequested: importM3uDialog.open()
            }

            // Content Stage — lazy view activation keeps unused views at 0 RAM
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Loader {
                    anchors.fill: parent
                    active: win.activeSection === "tracks"
                         || win.activeSection === "favorites"
                         || win.activeSection === "recent"
                         || win.activeSection === "playlist"
                    visible: active
                    sourceComponent: TracksView {
                        controller: player
                        lib: library
                        sectionKey: win.activeSection
                        activePlaylistId: win.activePlaylistId
                        sectionTitle: {
                            if (win.activeSection === "favorites") return qsTr("Favorites")
                            if (win.activeSection === "recent") return qsTr("Recently Played")
                            if (win.activeSection === "playlist") return win.activePlaylistName
                            return qsTr("All Tracks")
                        }
                        onAddFolderRequested: addFolderDialog.open()
                        onOpenFilesRequested: openAudioDialog.open()
                        onExportPlaylistRequested: (plId) => {
                            win.exportPlaylistTargetId = plId
                            exportM3uDialog.open()
                        }
                        onDeletePlaylistRequested: (plId) => {
                            library.deletePlaylist(plId)
                            win.activeSection = "tracks"
                            win.activePlaylistId = -1
                        }
                        onInspectTrack: (map) => win.inspectedTrackMap = map
                    }
                }

                Loader {
                    anchors.fill: parent
                    active: win.activeSection === "albums"
                    visible: active
                    sourceComponent: AlbumsView {
                        controller: player
                        lib: library
                    }
                }

                Loader {
                    anchors.fill: parent
                    active: win.activeSection === "artists"
                    visible: active
                    sourceComponent: ArtistsView {
                        controller: player
                        lib: library
                    }
                }

                Loader {
                    anchors.fill: parent
                    active: win.activeSection === "folders"
                    visible: active
                    sourceComponent: FoldersView {
                        controller: player
                        lib: library
                        onAddFolderRequested: addFolderDialog.open()
                    }
                }

                Loader {
                    anchors.fill: parent
                    active: win.activeSection === "settings"
                    visible: active
                    sourceComponent: SettingsView {
                        controller: player
                        lib: library
                        settingsObj: appSettings
                        themeObj: themeManager
                        onAddFolderRequested: addFolderDialog.open()
                    }
                }

                // Lazy-loaded Queue Drawer
                Loader {
                    active: win.queueOpen
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 12
                    sourceComponent: QueueDrawer {
                        controller: player
                        onCloseRequested: win.queueOpen = false
                    }
                }

                // Lazy-loaded 10-band EQ & DSP Drawer
                Loader {
                    active: win.eqOpen
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    sourceComponent: EqDrawer {
                        controller: player
                        settingsObj: appSettings
                        onCloseRequested: win.eqOpen = false
                    }
                }
            }
        }

        PlaybackBar {
            Layout.fillWidth: true
            controller: player
            lyricsOpen: win.lyricsOpen
            eqOpen: win.eqOpen
            queueOpen: win.queueOpen
            onToggleQueue: {
                win.queueOpen = !win.queueOpen
                if (win.queueOpen) win.eqOpen = false
            }
            onToggleLyrics: win.lyricsOpen = !win.lyricsOpen
            onToggleEq: {
                win.eqOpen = !win.eqOpen
                if (win.eqOpen) win.queueOpen = false
            }
            onOpenTrackInfo: win.inspectedTrackMap = player.currentTrackMap
            onToggleFavorite: {
                if (player.trackId >= 0) {
                    library.toggleFavorite(player.trackId, player.trackIsFavorite)
                }
            }
        }
    }

    // Lazy-loaded Immersive Now-Playing & Synced Lyrics Overlay
    Loader {
        active: win.lyricsOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: DesignTokens.playbackBarHeight
        sourceComponent: NowPlayingOverlay {
            controller: player
            lyricsCtrl: lyricsController
            settingsObj: appSettings
            onCloseRequested: win.lyricsOpen = false
        }
    }

    // Lazy-loaded Technical Track Metadata Inspector Modal
    Loader {
        active: win.inspectedTrackMap !== null
        anchors.fill: parent
        sourceComponent: TrackInfoDialog {
            trackMap: win.inspectedTrackMap || ({})
            onCloseRequested: win.inspectedTrackMap = null
        }
    }

    // Lazy-loaded Create Playlist Modal
    Loader {
        active: win.createPlaylistOpen
        anchors.fill: parent
        sourceComponent: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.45)
            MouseArea { anchors.fill: parent; onClicked: win.createPlaylistOpen = false }

            Rectangle {
                anchors.centerIn: parent
                width: 340; height: 158
                radius: DesignTokens.radiusLg
                color: DesignTokens.surfaceElevated
                border.width: 1; border.color: DesignTokens.borderHighlight
                MouseArea { anchors.fill: parent }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Text {
                        text: qsTr("New Playlist")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }
                    TextField {
                        id: plNameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Playlist name...")
                        focus: true
                        onAccepted: createBtnMa.doCreate()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitHeight: 30; implicitWidth: 68; radius: 15
                            color: DesignTokens.surfaceHover
                            Text { anchors.centerIn: parent; text: qsTr("Cancel"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 12 }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: win.createPlaylistOpen = false }
                        }
                        Rectangle {
                            implicitHeight: 30; implicitWidth: 76; radius: 15
                            color: DesignTokens.accent
                            Text { anchors.centerIn: parent; text: qsTr("Create"); color: "#ffffff"; font.family: DesignTokens.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                            MouseArea {
                                id: createBtnMa
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                function doCreate() {
                                    const n = plNameField.text.trim()
                                    if (n.length > 0) {
                                        library.createPlaylist(n)
                                        win.createPlaylistOpen = false
                                    }
                                }
                                onClicked: doCreate()
                            }
                        }
                    }
                }
            }
        }
    }

    // Lazy-loaded Mini Floating Pill Window (0 RAM when inactive)
    Loader {
        active: win.miniModeOpen
        sourceComponent: MiniPlayerWindow {
            controller: player
            onRestoreMainRequested: win.toggleMiniMode()
        }
    }

    // Global drag & drop overlay
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                player.openUrls(drop.urls, false)
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            radius: DesignTokens.radiusLg
            visible: parent.containsDrag
            color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.12)
            border.width: 2
            border.color: DesignTokens.accent

            Text {
                anchors.centerIn: parent
                text: qsTr("Drop audio files, folders, .cue, or .m3u8 to play")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
        }
    }

    // Transient toast pill
    Rectangle {
        visible: player.toastMessage.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: DesignTokens.playbackBarHeight + 16
        height: 36
        width: Math.min(parent.width - 64, toastText.implicitWidth + 32)
        radius: 18
        color: DesignTokens.surfaceElevated
        border.width: 1
        border.color: DesignTokens.borderHighlight

        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 24
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: player.toastMessage
            color: DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
        }

        Timer {
            interval: 3200
            running: player.toastMessage.length > 0
            onTriggered: player.clearToast()
        }
    }

    // Crisp outer border when unmaximized
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: win.visibility === Window.Maximized ? 0 : 1
        border.color: DesignTokens.borderSubtle
    }
}
