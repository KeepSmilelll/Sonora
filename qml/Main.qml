import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Sonora.UI

Window {
    id: win

    width: 1220
    height: 790
    minimumWidth: 920
    minimumHeight: 610
    visible: false
    title: "Sonora"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: DesignTokens.nativeBackdropActive ? "transparent" : DesignTokens.windowBg

    property string activeSection: "tracks"
    property int activePlaylistId: -1
    property string activePlaylistName: ""
    property bool queueOpen: false
    readonly property bool compactWindow: width < 1040
    readonly property bool queueInWorkspace: queueOpen || queueKeepAlive
    property bool lyricsOpen: false
    property bool eqOpen: false
    property bool miniModeOpen: false
    property bool forceQuit: false
    property var miniWindowObj: null
    property bool createPlaylistOpen: false
    property var inspectedTrackMap: null
    property int exportPlaylistTargetId: -1

    // Keep-alive flags so exit animations play smoothly before Loaders free RAM
    property bool lyricsKeepAlive: false
    property bool queueKeepAlive: false
    property bool eqKeepAlive: false
    property bool trackInfoKeepAlive: false
    property var cachedTrackMap: ({})
    property bool createModalKeepAlive: false

    function syncTitleBarDragState() {
        const allowDrag = !win.createPlaylistOpen && win.inspectedTrackMap === null
        platformBridge.setTitleBarDragEnabled(allowDrag)
    }

    onCreatePlaylistOpenChanged: {
        syncTitleBarDragState()
        if (createPlaylistOpen) {
            createCloseAnim.stop()
            createModalKeepAlive = true
            createOpenAnim.restart()
        } else if (createPlaylistLoader.active) {
            createOpenAnim.stop()
            createCloseAnim.restart()
        }
    }

    onInspectedTrackMapChanged: {
        syncTitleBarDragState()
        if (inspectedTrackMap !== null) {
            cachedTrackMap = inspectedTrackMap
            trackInfoCloseAnim.stop()
            trackInfoKeepAlive = true
            trackInfoOpenAnim.restart()
        } else if (trackInfoLoader.active) {
            trackInfoOpenAnim.stop()
            trackInfoCloseAnim.restart()
        }
    }

    onLyricsOpenChanged: {
        if (lyricsOpen) {
            win.queueOpen = false
            lyricsCloseAnim.stop()
            lyricsKeepAlive = true
            lyricsOpenAnim.restart()
        } else if (lyricsLoader.active) {
            lyricsOpenAnim.stop()
            lyricsCloseAnim.restart()
        }
    }

    onQueueOpenChanged: {
        if (queueOpen) {
            win.lyricsOpen = false
            queueCloseAnim.stop()
            queueKeepAlive = true
            queueOpenAnim.restart()
        } else if (queueLoader.active) {
            queueOpenAnim.stop()
            queueCloseAnim.restart()
        }
    }

    onEqOpenChanged: {
        if (eqOpen) {
            eqCloseAnim.stop()
            eqKeepAlive = true
            eqOpenAnim.restart()
        } else if (eqLoader.active) {
            eqOpenAnim.stop()
            eqCloseAnim.restart()
        }
    }

    function triggerViewTransition() {
        viewSwitchAnim.restart()
    }

    onMiniModeOpenChanged: {
        if (miniModeOpen) {
            if (!miniWindowObj) {
                miniWindowObj = miniPlayerComponent.createObject(null, { controller: player })
                if (miniWindowObj) {
                    miniWindowObj.restoreMainRequested.connect(win.restoreMainWindow)
                    miniWindowObj.show()
                    miniWindowObj.raise()
                    miniWindowObj.requestActivate()
                }
            }
        } else if (miniWindowObj) {
            const w = miniWindowObj
            miniWindowObj = null
            w.hide()
            w.destroy()
        }
    }

    Component.onDestruction: {
        if (miniWindowObj) {
            miniWindowObj.destroy()
            miniWindowObj = null
        }
    }

    function syncThemeTokens() {
        DesignTokens.isDark = themeManager.isDark
        DesignTokens.nativeBackdropActive = themeManager.nativeBackdropActive
        DesignTokens.backdropMode = themeManager.backdropMode
        DesignTokens.ambientPrimary = themeManager.ambientPrimary
        DesignTokens.ambientSecondary = themeManager.ambientSecondary
        DesignTokens.accent = themeManager.accentColor
    }

    function restoreMainWindow() {
        if (win.visibility === Window.Minimized) {
            win.showNormal()
        } else if (!win.visible) {
            win.show()
        }
        win.raise()
        win.requestActivate()
        win.miniModeOpen = false
    }

    function toggleMiniMode() {
        if (!miniModeOpen) {
            miniModeOpen = true
            win.hide()
        } else {
            restoreMainWindow()
        }
    }

    Component.onCompleted: {
        syncThemeTokens()
        syncTitleBarDragState()
    }

    onClosing: (close) => {
        if (forceQuit || platformBridge.quitting) {
            close.accepted = true
            return
        }
        if (Qt.platform.os === "windows" && appSettings.closeToTray) {
            close.accepted = false
            win.hide()
            return
        }
        close.accepted = true
        platformBridge.requestQuit()
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
        function onShowMainWindowRequested() { win.restoreMainWindow() }
    }

    AmbientBackdrop {
        anchors.fill: parent
        opacity: DesignTokens.nativeBackdropActive ? 0.35 : 0.12
        playing: player.isPlaying
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

    // Top Spatial Frameless TitleBar (always uncovered for native OS drag & caption controls)
    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: DesignTokens.titleBarHeight
        z: 30
        compact: win.compactWindow
        searchText: library.searchQuery
        isMaximized: win.visibility === Window.Maximized
        onSearchEdited: (text) => library.searchQuery = text
        onOpenFilesRequested: openAudioDialog.open()
        onToggleTheme: themeManager.toggleDarkLight()
        onToggleMiniMode: win.toggleMiniMode()
        onMinimizeClicked: win.showMinimized()
        onMaximizeRestoreClicked: platformBridge.toggleMaximize(win)
        onCloseClicked: win.close()
    }

    // Spatial Workspace Islands (Sidebar Rail + Rounded Content Stage)
    RowLayout {
        id: workspaceRow
        anchors.top: titleBar.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: win.queueInWorkspace ? 16 + queueHost.width + 12 : 16
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 18
        spacing: win.compactWindow ? 12 : 18
        z: 5

        Sidebar {
            compact: win.compactWindow && win.queueInWorkspace
            Layout.fillHeight: true
            activeSection: win.activeSection
            customPlaylists: library.playlists
            activePlaylistId: win.activePlaylistId
            onSectionSelected: (section) => {
                const changed = (win.activeSection !== section || win.activePlaylistId !== -1)
                win.activeSection = section
                win.activePlaylistId = -1
                if (win.lyricsOpen) win.lyricsOpen = false
                if (section === "tracks" || section === "favorites" || section === "recent" || section === "folders") {
                    library.loadSection(section)
                }
                if (changed) win.triggerViewTransition()
            }
            onPlaylistSelected: (plId, plName) => {
                const changed = (win.activeSection !== "playlist" || win.activePlaylistId !== plId)
                win.activeSection = "playlist"
                win.activePlaylistId = plId
                win.activePlaylistName = plName
                if (win.lyricsOpen) win.lyricsOpen = false
                library.openPlaylist(plId)
                if (changed) win.triggerViewTransition()
            }
            onCreatePlaylistRequested: win.createPlaylistOpen = true
            onImportM3uRequested: importM3uDialog.open()
        }

        // Rounded Glass Content Stage — lazy view activation keeps unused views at 0 RAM
        GlassPanel {
            id: contentStage
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: DesignTokens.radiusXl
            readonly property bool tracksVisible: win.activeSection === "tracks"
                || win.activeSection === "favorites" || win.activeSection === "recent"
                || win.activeSection === "playlist"
            panelColor: tracksVisible ? "transparent" : DesignTokens.canvasBg
            borderColor: tracksVisible ? "transparent" : DesignTokens.borderOuter
            specularEnabled: !tracksVisible
            shadowEnabled: false

            Item {
                id: stageViewport
                anchors.fill: parent
                transform: Translate { id: stageShift; y: 0 }

                ParallelAnimation {
                    id: viewSwitchAnim
                    NumberAnimation { target: stageViewport; property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
                    NumberAnimation { target: stageShift; property: "y"; from: 10; to: 0; duration: 240; easing.type: Easing.OutQuint }
                }

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
                            win.triggerViewTransition()
                        }
                        onInspectTrack: (map) => win.inspectedTrackMap = map
                        onShowLyricsRequested: win.lyricsOpen = true
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
            }
        }
    }

    // Immersive Now-Playing & Synced Lyrics Spatial Sheet
    // Anchored below TitleBar and above Floating Playback Dock so drag & transport remain active
    Item {
        id: lyricsSheetHost
        visible: lyricsLoader.active
        anchors.top: titleBar.bottom
        anchors.topMargin: 10
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 18
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        z: 16
        transformOrigin: Item.Bottom
        transform: Translate { id: lyricsShift; y: 0 }

        ParallelAnimation {
            id: lyricsOpenAnim
            NumberAnimation { target: lyricsSheetHost; property: "opacity"; from: 0.0; to: 1.0; duration: 260; easing.type: Easing.OutCubic }
            NumberAnimation { target: lyricsSheetHost; property: "scale"; from: 0.965; to: 1.0; duration: DesignTokens.animSheet; easing.type: Easing.OutQuint }
            NumberAnimation { target: lyricsShift; property: "y"; from: 38; to: 0; duration: DesignTokens.animSheet; easing.type: Easing.OutQuint }
        }

        ParallelAnimation {
            id: lyricsCloseAnim
            NumberAnimation { target: lyricsSheetHost; property: "opacity"; to: 0.0; duration: 190; easing.type: Easing.InCubic }
            NumberAnimation { target: lyricsSheetHost; property: "scale"; to: 0.97; duration: 190; easing.type: Easing.InCubic }
            NumberAnimation { target: lyricsShift; property: "y"; to: 32; duration: 190; easing.type: Easing.InCubic }
            onFinished: {
                if (!win.lyricsOpen) win.lyricsKeepAlive = false
            }
        }

        Loader {
            id: lyricsLoader
            anchors.fill: parent
            active: win.lyricsOpen || win.lyricsKeepAlive
            sourceComponent: NowPlayingOverlay {
                controller: player
                lyricsCtrl: lyricsController
                settingsObj: appSettings
                onCloseRequested: win.lyricsOpen = false
                onToggleFavorite: win.toggleCurrentFavorite()
            }
        }
    }

    // Click-outside catcher for the floating EQ drawer.
    MouseArea {
        visible: win.eqOpen
        anchors.top: titleBar.bottom
        anchors.bottom: playbackDock.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 18
        onClicked: win.eqOpen = false
    }

    // The lazy queue shares the workspace with the library.
    Item {
        id: queueHost
        visible: queueLoader.active
        anchors.top: titleBar.bottom
        anchors.topMargin: 12
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 18
        anchors.right: parent.right
        anchors.rightMargin: 16
        width: 282
        z: 20
        transform: Translate { id: queueShift; x: 0 }

        ParallelAnimation {
            id: queueOpenAnim
            NumberAnimation { target: queueHost; property: "opacity"; from: 0.0; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: queueShift; property: "x"; from: 42; to: 0; duration: DesignTokens.animPanel; easing.type: Easing.OutQuint }
        }

        ParallelAnimation {
            id: queueCloseAnim
            NumberAnimation { target: queueHost; property: "opacity"; to: 0.0; duration: 160; easing.type: Easing.InCubic }
            NumberAnimation { target: queueShift; property: "x"; to: 36; duration: 160; easing.type: Easing.InCubic }
            onFinished: {
                if (!win.queueOpen) win.queueKeepAlive = false
            }
        }

        Loader {
            id: queueLoader
            anchors.fill: parent
            active: win.queueOpen || win.queueKeepAlive
            sourceComponent: QueueDrawer {
                controller: player
                onCloseRequested: win.queueOpen = false
            }
        }
    }

    // Lazy-loaded Floating 10-band EQ & DSP Drawer (springs up above dock)
    Item {
        id: eqHost
        visible: eqLoader.active
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 16
        width: eqLoader.item ? eqLoader.item.implicitWidth : 548
        height: eqLoader.item ? eqLoader.item.implicitHeight : 344
        z: 20
        transformOrigin: Item.BottomRight
        transform: Translate { id: eqShift; y: 0 }

        ParallelAnimation {
            id: eqOpenAnim
            NumberAnimation { target: eqHost; property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { target: eqHost; property: "scale"; from: 0.94; to: 1.0; duration: 260; easing.type: Easing.OutBack; easing.overshoot: 1.06 }
            NumberAnimation { target: eqShift; property: "y"; from: 24; to: 0; duration: 260; easing.type: Easing.OutQuint }
        }

        ParallelAnimation {
            id: eqCloseAnim
            NumberAnimation { target: eqHost; property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: eqHost; property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: eqShift; property: "y"; to: 18; duration: 150; easing.type: Easing.InQuad }
            onFinished: {
                if (!win.eqOpen) win.eqKeepAlive = false
            }
        }

        Loader {
            id: eqLoader
            anchors.fill: parent
            active: win.eqOpen || win.eqKeepAlive
            sourceComponent: EqDrawer {
                controller: player
                settingsObj: appSettings
                onCloseRequested: win.eqOpen = false
            }
        }
    }

    // Floating Capsule Playback Dock
    PlaybackBar {
        id: playbackDock
        anchors.left: parent.left
        anchors.leftMargin: DesignTokens.dockMarginX
        anchors.right: parent.right
        anchors.rightMargin: DesignTokens.dockMarginX
        anchors.bottom: parent.bottom
        anchors.bottomMargin: DesignTokens.dockMarginBottom
        height: DesignTokens.dockHeight
        z: 25
        controller: player
        compact: win.compactWindow
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
        onToggleFavorite: win.toggleCurrentFavorite()
    }

    function toggleCurrentFavorite() {
        if (player.trackId >= 0)
            library.toggleFavorite(player.trackId, player.trackIsFavorite)
    }

    // Lazy-loaded Technical Track Metadata Inspector Modal
    Item {
        id: trackInfoHost
        visible: trackInfoLoader.active
        anchors.fill: parent
        z: 40

        ParallelAnimation {
            id: trackInfoOpenAnim
            NumberAnimation { target: trackInfoHost; property: "opacity"; from: 0.0; to: 1.0; duration: 190; easing.type: Easing.OutCubic }
            NumberAnimation { target: trackInfoLoader; property: "scale"; from: 0.94; to: 1.0; duration: 240; easing.type: Easing.OutBack; easing.overshoot: 1.06 }
        }

        ParallelAnimation {
            id: trackInfoCloseAnim
            NumberAnimation { target: trackInfoHost; property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: trackInfoLoader; property: "scale"; to: 0.96; duration: 150; easing.type: Easing.InQuad }
            onFinished: {
                if (win.inspectedTrackMap === null) win.trackInfoKeepAlive = false
            }
        }

        Loader {
            id: trackInfoLoader
            anchors.fill: parent
            active: win.inspectedTrackMap !== null || win.trackInfoKeepAlive
            sourceComponent: TrackInfoDialog {
                trackMap: win.inspectedTrackMap || win.cachedTrackMap
                onCloseRequested: win.inspectedTrackMap = null
            }
        }
    }

    // Lazy-loaded Spatial Create Playlist Modal
    Item {
        id: createModalHost
        visible: createPlaylistLoader.active
        anchors.fill: parent
        z: 40

        ParallelAnimation {
            id: createOpenAnim
            NumberAnimation { target: createModalHost; property: "opacity"; from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: createPlaylistLoader; property: "scale"; from: 0.93; to: 1.0; duration: 240; easing.type: Easing.OutBack; easing.overshoot: 1.06 }
        }

        ParallelAnimation {
            id: createCloseAnim
            NumberAnimation { target: createModalHost; property: "opacity"; to: 0.0; duration: 140; easing.type: Easing.InQuad }
            NumberAnimation { target: createPlaylistLoader; property: "scale"; to: 0.95; duration: 140; easing.type: Easing.InQuad }
            onFinished: {
                if (!win.createPlaylistOpen) win.createModalKeepAlive = false
            }
        }

        Loader {
            id: createPlaylistLoader
            anchors.fill: parent
            active: win.createPlaylistOpen || win.createModalKeepAlive
            sourceComponent: Item {
                Rectangle {
                    anchors.fill: parent
                    color: DesignTokens.surfaceScrim
                    MouseArea { anchors.fill: parent; onClicked: win.createPlaylistOpen = false }
                }

                GlassPanel {
                    anchors.centerIn: parent
                    width: 372
                    height: 184
                    radius: DesignTokens.radiusXl
                    elevated: true
                    shadowEnabled: true

                    MouseArea { anchors.fill: parent }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 14

                        RowLayout {
                            spacing: 9
                            Rectangle {
                                implicitWidth: 28; implicitHeight: 28; radius: 8
                                color: DesignTokens.accentSoft
                                IconGlyph { anchors.centerIn: parent; name: "playlist"; size: 15; color: DesignTokens.accent }
                            }
                            Text {
                                text: qsTr("New Playlist")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 38
                            radius: 11
                            color: DesignTokens.surfaceInput
                            border.width: 1
                            border.color: plNameInput.activeFocus ? DesignTokens.accent : DesignTokens.borderOuter
                            Behavior on border.color { ColorAnimation { duration: 140 } }

                            TextInput {
                                id: plNameInput
                                anchors.fill: parent
                                anchors.leftMargin: 13
                                anchors.rightMargin: 13
                                verticalAlignment: TextInput.AlignVCenter
                                color: DesignTokens.textPrimary
                                selectionColor: DesignTokens.accent
                                selectedTextColor: "#ffffff"
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                focus: true
                                clip: true
                                onAccepted: doCreatePlaylist()
                                Component.onCompleted: forceActiveFocus()

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Enter playlist name...")
                                    color: DesignTokens.textTertiary
                                    font: plNameInput.font
                                    visible: plNameInput.text.length === 0
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Item { Layout.fillWidth: true }

                            SonoraButton {
                                compact: true
                                text: qsTr("Cancel")
                                onClicked: win.createPlaylistOpen = false
                            }

                            SonoraButton {
                                id: createConfirmBtn
                                compact: true
                                primary: true
                                text: qsTr("Create")
                                enabled: plNameInput.text.trim().length > 0
                                onClicked: doCreatePlaylist()
                            }
                        }
                    }

                    function doCreatePlaylist() {
                        const n = plNameInput.text.trim()
                        if (n.length > 0) {
                            library.createPlaylist(n)
                            win.createPlaylistOpen = false
                        }
                    }
                }
            }
        }
    }

    // Lazy-created independent top-level Mini Floating Pill Window (0 RAM when inactive;
    // created with null parent so hiding main window to tray never hides the mini player)
    Component {
        id: miniPlayerComponent
        MiniPlayerWindow {}
    }

    // Global drag & drop overlay
    DropArea {
        anchors.fill: parent
        z: 45
        onDropped: (drop) => {
            if (drop.hasUrls) {
                player.openUrls(drop.urls, false)
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 12
            radius: DesignTokens.radiusXl
            visible: opacity > 0.01
            opacity: parent.containsDrag ? 1.0 : 0.0
            color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.14)
            border.width: 2
            border.color: DesignTokens.accent
            Behavior on opacity { NumberAnimation { duration: 150 } }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 52; implicitHeight: 52; radius: 16
                    color: DesignTokens.accentSoft
                    IconGlyph { anchors.centerIn: parent; name: "music"; size: 24; color: DesignTokens.accent }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Drop audio files, folders, .cue, or .m3u8 to play")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    // Floating spatial toast capsule
    GlassPanel {
        id: toastPill
        readonly property bool shown: player.toastMessage.length > 0
        visible: opacity > 0.01
        opacity: shown ? 1.0 : 0.0
        z: 35
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 14
        height: 38
        width: Math.min(parent.width - 64, toastText.implicitWidth + 36)
        radius: 19
        elevated: true
        shadowEnabled: true
        transform: Translate { y: toastPill.shown ? 0 : 10 }

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 28
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: player.toastMessage
            color: DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
            font.weight: Font.Medium
        }

        Timer {
            interval: 3200
            running: player.toastMessage.length > 0
            onTriggered: player.clearToast()
        }
    }

    // Subtle outer window rim when unmaximized
    Rectangle {
        anchors.fill: parent
        z: 50
        color: "transparent"
        border.width: win.visibility === Window.Maximized ? 0 : 1
        border.color: DesignTokens.borderOuter
    }
}
