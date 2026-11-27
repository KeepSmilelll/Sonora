import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Virela.UI

Window {
    id: win

    width: 1240
    height: 800
    minimumWidth: 920
    minimumHeight: 610
    visible: false
    title: "Virela"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: DesignTokens.nativeBackdropActive ? "transparent" : DesignTokens.windowBg

    property string activeSection: "tracks"
    property int activePlaylistId: -1
    property string activePlaylistName: ""

    // Continuous Spatial Stage state ("collapsed" | "rail" | "immersive")
    property string stageMode: "rail"
    property string previousNonImmersiveMode: "rail"
    property string stageTab: "lyrics" // "lyrics" | "queue"
    property bool railExpanded: false
    property real stageRailWidth: DesignTokens.stageDefaultWidth

    readonly property bool narrowWindow: width < 980
    readonly property bool compactWindow: width < 1040
    readonly property string effectiveStageMode: (stageMode === "rail" && narrowWindow) ? "collapsed" : stageMode
    readonly property bool lyricsOpen: effectiveStageMode !== "collapsed" && stageTab === "lyrics"
    readonly property bool queueOpen: effectiveStageMode !== "collapsed" && stageTab === "queue"

    property real animImmersive: stageMode === "immersive" ? 1.0 : 0.0
    Behavior on animImmersive {
        NumberAnimation { duration: DesignTokens.animStage; easing.type: Easing.OutQuint }
    }

    property bool eqOpen: false
    property bool miniModeOpen: false
    property bool forceQuit: false
    property var miniWindowObj: null
    property bool createPlaylistOpen: false
    property var inspectedTrackMap: null
    property int exportPlaylistTargetId: -1

    // Keep-alive flags so exit animations play smoothly before Loaders free RAM
    property bool eqKeepAlive: false
    property bool trackInfoKeepAlive: false
    property var cachedTrackMap: ({})
    property bool createModalKeepAlive: false

    function syncTitleBarDragState() {
        const allowDrag = !win.createPlaylistOpen && win.inspectedTrackMap === null
        platformBridge.setTitleBarDragEnabled(allowDrag)
    }

    function dismissSearchIfOutside(wx, wy) {
        if (titleBar.searchFocused && !titleBar.isPointInsideSearch(wx, wy)) {
            titleBar.clearSearchFocus()
        }
    }

    function dismissSearchFocus() {
        titleBar.clearSearchFocus()
    }

    function enterImmersive(sourceItemHint) {
        if (win.stageMode === "immersive") return
        win.previousNonImmersiveMode = win.effectiveStageMode === "collapsed" ? "collapsed" : "rail"
        const fromDock = (sourceItemHint === playbackDock.coverAnchorItem()) || (win.effectiveStageMode === "collapsed" && !sourceItemHint)
        const src = sourceItemHint
            || (win.effectiveStageMode === "rail" ? focusStage.currentCoverItem() : playbackDock.coverAnchorItem())
        const fromR = src ? src.width * 0.5 : 23
        const predImm = focusStage.computeImmersiveCoverGeometry(
            win.width, win.height, Boolean(appSettings.visualizerEnabled))
        heroLayer.startHero({
            tag: "nowplaying-cover",
            vinylMode: true,
            startRotation: fromDock ? playbackDock.currentVinylAngle() : focusStage.sharedVinylAngle,
            spinSpeed: player.isPlaying ? 24.0 : 0.0,
            sourceItem: src,
            fromRadius: fromR,
            toRect: {
                x: predImm.x,
                y: predImm.y,
                width: predImm.width,
                height: predImm.height
            },
            toRadius: predImm.radius,
            imageUrl: player.trackCoverUrl
        })
        win.stageMode = "immersive"
    }

    function exitImmersive() {
        if (win.stageMode !== "immersive") return
        const targetMode = (win.previousNonImmersiveMode === "collapsed" || win.narrowWindow) ? "collapsed" : "rail"
        const src = focusStage.currentCoverItem()
        let toRect = null
        let toR = 23

        if (targetMode === "rail") {
            const railW = Math.min(win.stageRailWidth, Math.max(DesignTokens.stageMinWidth, (win.width - 32) * 0.42))
            const rg = focusStage.computeRailCoverGeometry(railW)
            toRect = {
                x: Math.round(win.width - 16 - railW + rg.x),
                y: DesignTokens.titleBarHeight + 10 + rg.y,
                width: rg.width,
                height: rg.height
            }
            toR = rg.radius
        } else {
            const targetDockW = Math.min(win.width - 32, DesignTokens.dockMaxWidth)
            toRect = playbackDock.predictedCoverRectInWindow(
                win.width, win.height, targetDockW, win.compactWindow)
            toR = toRect.radius
        }

        heroLayer.startHero({
            tag: "nowplaying-cover",
            vinylMode: true,
            startRotation: focusStage.sharedVinylAngle,
            spinSpeed: player.isPlaying ? 24.0 : 0.0,
            sourceItem: src,
            fromRadius: focusStage.currentCoverRadius(),
            toRect: toRect,
            toRadius: toR,
            imageUrl: player.trackCoverUrl
        })
        win.stageMode = targetMode
    }

    function toggleImmersive(sourceItemHint) {
        if (win.stageMode === "immersive") {
            exitImmersive()
        } else {
            enterImmersive(sourceItemHint)
        }
    }

    function toggleStageLyrics(sourceItemHint) {
        win.eqOpen = false
        if (win.stageMode === "immersive") {
            if (win.stageTab !== "lyrics") {
                win.stageTab = "lyrics"
            } else {
                exitImmersive()
            }
            return
        }
        if (win.effectiveStageMode === "collapsed") {
            win.stageTab = "lyrics"
            if (win.narrowWindow) {
                enterImmersive(sourceItemHint || playbackDock.coverAnchorItem())
            } else {
                win.stageMode = "rail"
            }
            return
        }
        if (win.stageTab !== "lyrics") {
            win.stageTab = "lyrics"
        } else {
            enterImmersive(sourceItemHint)
        }
    }

    function toggleStageQueue() {
        win.eqOpen = false
        if (win.stageMode === "immersive") {
            if (win.stageTab !== "queue") {
                win.stageTab = "queue"
            } else {
                exitImmersive()
            }
            return
        }
        if (win.effectiveStageMode === "collapsed") {
            win.stageTab = "queue"
            if (win.narrowWindow) {
                enterImmersive(playbackDock.coverAnchorItem())
            } else {
                win.stageMode = "rail"
            }
            return
        }
        if (win.stageTab !== "queue") {
            win.stageTab = "queue"
        } else {
            win.stageMode = "collapsed"
        }
    }

    function toggleStagePanel() {
        if (win.stageMode === "immersive") {
            exitImmersive()
        } else if (win.effectiveStageMode === "collapsed") {
            if (win.narrowWindow) {
                enterImmersive(playbackDock.coverAnchorItem())
            } else {
                win.stageMode = "rail"
            }
        } else {
            win.stageMode = "collapsed"
        }
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
            trackInfoKeepAlive = true
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
                miniWindowObj = miniPlayerComponent.createObject(null, {
                    controller: player,
                    lyricsCtrl: lyricsController,
                    settingsObj: appSettings
                })
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

    Component.onCompleted: syncTitleBarDragState()

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
        target: platformBridge
        function onToggleMiniModeRequested() { win.toggleMiniMode() }
        function onShowMainWindowRequested() { win.restoreMainWindow() }
    }

    Shortcut {
        sequence: "Escape"
        enabled: win.createPlaylistOpen || win.inspectedTrackMap !== null || win.eqOpen || win.stageMode === "immersive"
        onActivated: {
            if (win.createPlaylistOpen) win.createPlaylistOpen = false
            else if (win.inspectedTrackMap !== null) win.inspectedTrackMap = null
            else if (win.eqOpen) win.eqOpen = false
            else if (win.stageMode === "immersive") win.exitImmersive()
        }
    }

    AmbientBackdrop {
        anchors.fill: parent
        immersive: win.stageMode === "immersive"
        immersiveFactor: win.animImmersive
        playing: player.isPlaying
        audioEnergy: focusStage.liveBassEnergy
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

    // Top Spatial Frameless TitleBar (recedes smoothly when Immersive stage expands over it)
    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: DesignTokens.titleBarHeight
        z: 30
        opacity: Math.max(0.0, 1.0 - win.animImmersive * 1.8)
        visible: opacity > 0.01
        enabled: win.stageMode !== "immersive"
        compact: win.compactWindow
        searchText: library.searchQuery
        isMaximized: win.visibility === Window.Maximized
        railExpanded: win.railExpanded
        stageMode: win.effectiveStageMode
        immersiveFactor: win.animImmersive
        onToggleRailExpanded: win.railExpanded = !win.railExpanded
        onToggleStagePanel: win.toggleStagePanel()
        onSearchEdited: (text) => library.searchQuery = text
        onOpenFilesRequested: openAudioDialog.open()
        onToggleTheme: themeManager.toggleDarkLight()
        onToggleMiniMode: win.toggleMiniMode()
        onMinimizeClicked: win.showMinimized()
        onMaximizeRestoreClicked: platformBridge.toggleMaximize(win)
        onCloseClicked: win.close()
    }

    // Continuous 3-Zone Spatial Workspace (NavigationRail + ContentStage + FocusStage)
    Item {
        id: workspaceHost
        anchors.top: titleBar.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: playbackDock.top
        anchors.bottomMargin: 14
        z: 10

        // Browse Layer (Rail + Library Canvas); smoothly recesses when FocusStage expands full-screen
        Item {
            id: browseLayer
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: win.effectiveStageMode === "rail" ? (focusStage.width + 14) : 0
            opacity: Math.max(0.0, 1.0 - win.animImmersive * 1.25)
            visible: opacity > 0.01
            enabled: win.stageMode !== "immersive"
            scale: 1.0 - 0.035 * win.animImmersive
            transform: Translate { x: -18 * win.animImmersive }

            RowLayout {
                anchors.fill: parent
                spacing: win.compactWindow ? 12 : 16

                NavigationRail {
                    id: navRail
                    Layout.fillHeight: true
                    z: 20
                    expanded: win.railExpanded
                    activeSection: win.activeSection
                    customPlaylists: library.playlists
                    activePlaylistId: win.activePlaylistId
                    onToggleExpandedRequested: win.railExpanded = !win.railExpanded
                    onSectionSelected: (section) => {
                        const changed = (win.activeSection !== section || win.activePlaylistId !== -1)
                        win.activeSection = section
                        win.activePlaylistId = -1
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
                        library.openPlaylist(plId)
                        if (changed) win.triggerViewTransition()
                    }
                    onCreatePlaylistRequested: win.createPlaylistOpen = true
                    onImportM3uRequested: importM3uDialog.open()
                }

                Rectangle {
                    id: contentStage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: DesignTokens.radiusXl
                    readonly property bool tracksVisible: win.activeSection === "tracks"
                        || win.activeSection === "favorites" || win.activeSection === "recent"
                        || win.activeSection === "playlist"
                    color: tracksVisible ? "transparent" : DesignTokens.contentStageBg
                    border.width: tracksVisible ? 0 : 1
                    border.color: DesignTokens.hairlineBorder
                    clip: true

                    Item {
                        id: stageViewport
                        anchors.fill: parent
                        transform: Translate { id: stageViewportShift; y: 0 }

                        ParallelAnimation {
                            id: viewSwitchAnim
                            NumberAnimation {
                                target: stageViewport
                                property: "opacity"
                                from: 0.0
                                to: 1.0
                                duration: 190
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: stageViewportShift
                                property: "y"
                                from: 10
                                to: 0
                                duration: 240
                                easing.type: Easing.OutQuint
                            }
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
        }
    }

    // Continuous Focus Stage (Collapsed <-> Companion Rail <-> 100% Full-Window Immersive covering TitleBar & Dock)
    FocusStage {
        id: focusStage
        anchors.top: parent.top
        anchors.topMargin: Math.round((DesignTokens.titleBarHeight + 10) * (1.0 - win.animImmersive))
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round((DesignTokens.dockHeight + DesignTokens.dockMarginBottom + 14) * (1.0 - win.animImmersive))
        anchors.right: parent.right
        anchors.rightMargin: Math.round(16 * (1.0 - win.animImmersive))
        z: win.animImmersive > 0.02 ? 32 : 15
        controller: player
        lyricsCtrl: lyricsController
        settingsObj: appSettings
        stageMode: win.effectiveStageMode
        immersiveFactor: win.animImmersive
        activeTab: win.stageTab
        eqOpen: win.eqOpen
        hideCoverForHero: heroLayer.isConcealing("nowplaying-cover")
        onActiveTabChanged: {
            if (win.stageTab !== activeTab) win.stageTab = activeTab
        }

        width: {
            if (win.effectiveStageMode === "immersive") return win.width
            if (win.effectiveStageMode === "rail") {
                return Math.min(win.stageRailWidth, Math.max(DesignTokens.stageMinWidth, (win.width - 32) * 0.42))
            }
            return 0
        }
        opacity: win.effectiveStageMode === "collapsed" ? 0.0 : 1.0
        visible: width > 2

        Behavior on width {
            enabled: !focusStage.draggingResize
            NumberAnimation { duration: DesignTokens.animStage; easing.type: Easing.OutQuint }
        }
        Behavior on opacity {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        onRequestImmersiveToggle: win.toggleImmersive(focusStage.currentCoverItem())
        onRequestCollapse: win.stageMode = "collapsed"
        onToggleFavorite: win.toggleCurrentFavorite()
        onToggleEq: win.eqOpen = !win.eqOpen
        onOpenTrackInfo: win.inspectedTrackMap = player.currentTrackMap
        onDragResize: (deltaX) => {
            win.stageRailWidth = Math.max(120, Math.min(workspaceHost.width * 0.72, win.stageRailWidth - deltaX))
        }
        onDragResizeFinished: (currentW) => {
            if (currentW < 210) {
                win.stageMode = "collapsed"
                win.stageRailWidth = DesignTokens.stageDefaultWidth
            } else if (currentW > Math.min(540, workspaceHost.width * 0.56)) {
                win.stageRailWidth = DesignTokens.stageDefaultWidth
                win.enterImmersive(focusStage.currentCoverItem())
            } else {
                win.stageRailWidth = Math.max(DesignTokens.stageMinWidth, Math.min(DesignTokens.stageMaxWidth, currentW))
            }
        }
    }

    // Click-outside catcher for the floating EQ drawer
    MouseArea {
        visible: win.eqOpen
        anchors.top: win.stageMode === "immersive" ? parent.top : titleBar.bottom
        anchors.bottom: win.stageMode === "immersive" ? parent.bottom : playbackDock.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 33
        onClicked: win.eqOpen = false
    }

    // Lazy-loaded Floating 10-band EQ & DSP Drawer
    Item {
        id: eqHost
        visible: eqLoader.active
        x: win.stageMode === "immersive"
            ? Math.max(20, Math.min(parent.width - width - 20, Math.round((focusStage.immersiveLeftColWidth - width) * 0.5) + 16))
            : Math.round(playbackDock.x + playbackDock.width - width)
        y: win.stageMode === "immersive"
            ? Math.max(56, parent.height - height - 24)
            : (playbackDock.y - height - 12)
        width: eqLoader.item ? eqLoader.item.implicitWidth : 548
        height: eqLoader.item ? eqLoader.item.implicitHeight : 344
        z: 34
        transformOrigin: win.stageMode === "immersive" ? Item.BottomLeft : Item.BottomRight
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

    // Centered Floating Morphing Capsule Playback Dock (recedes in Immersive mode where FocusStage embeds its own console)
    PlaybackBar {
        id: playbackDock
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: DesignTokens.dockMarginBottom
        width: Math.min(parent.width - 32,
                        win.stageMode === "immersive" ? DesignTokens.dockImmersiveMaxWidth : DesignTokens.dockMaxWidth)
        height: DesignTokens.dockHeight
        z: 35
        opacity: Math.max(0.0, 1.0 - win.animImmersive * 1.6)
        visible: opacity > 0.01
        enabled: win.stageMode !== "immersive"
        transform: Translate { y: Math.round(win.animImmersive * 32) }

        Behavior on width {
            NumberAnimation { duration: 300; easing.type: Easing.OutQuint }
        }

        controller: player
        compact: win.compactWindow
        lyricsOpen: win.lyricsOpen
        eqOpen: win.eqOpen
        queueOpen: win.queueOpen
        stageMode: win.effectiveStageMode
        stageTab: win.stageTab
        immersiveFactor: win.animImmersive
        hideCoverForHero: heroLayer.isConcealing("nowplaying-cover")
        onToggleQueue: win.toggleStageQueue()
        onToggleLyrics: win.toggleStageLyrics(playbackDock.coverAnchorItem())
        onCoverClicked: win.toggleImmersive(playbackDock.coverAnchorItem())
        onToggleEq: win.eqOpen = !win.eqOpen
        onOpenTrackInfo: win.inspectedTrackMap = player.currentTrackMap
        onToggleFavorite: win.toggleCurrentFavorite()
    }

    function toggleCurrentFavorite() {
        if (player.trackId >= 0)
            library.toggleFavorite(player.trackId, player.trackIsFavorite)
    }

    // Global Shared-Element Hero Transition Layer
    HeroTransitionLayer {
        id: heroLayer
        anchors.fill: parent
        z: 60
    }

    // Lazy-loaded Technical Track Metadata Inspector Modal
    Item {
        id: trackInfoHost
        visible: trackInfoLoader.active
        anchors.fill: parent
        z: 45

        Loader {
            id: trackInfoLoader
            anchors.fill: parent
            active: win.inspectedTrackMap !== null || win.trackInfoKeepAlive
            sourceComponent: TrackInfoDialog {
                open: win.inspectedTrackMap !== null
                trackMap: win.inspectedTrackMap || win.cachedTrackMap
                onCloseRequested: win.inspectedTrackMap = null
                onClosed: {
                    if (win.inspectedTrackMap === null) win.trackInfoKeepAlive = false
                }
            }
        }
    }

    // Lazy-loaded Spatial Create Playlist Modal
    Item {
        id: createModalHost
        visible: createPlaylistLoader.active
        anchors.fill: parent
        z: 45

        property real modalProgress: 0.0

        NumberAnimation {
            id: createOpenAnim
            target: createModalHost
            property: "modalProgress"
            to: 1.0
            duration: 210
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            id: createCloseAnim
            target: createModalHost
            property: "modalProgress"
            to: 0.0
            duration: 150
            easing.type: Easing.InQuad
            onFinished: {
                if (!win.createPlaylistOpen) win.createModalKeepAlive = false
            }
        }

        Rectangle {
            anchors.fill: parent
            color: DesignTokens.surfaceScrim
            opacity: createModalHost.modalProgress
            MouseArea { anchors.fill: parent; onClicked: win.createPlaylistOpen = false }
        }

        Loader {
            id: createPlaylistLoader
            anchors.fill: parent
            active: win.createPlaylistOpen || win.createModalKeepAlive
            sourceComponent: Item {
                GlassPanel {
                    anchors.centerIn: parent
                    width: 372
                    height: 184
                    radius: DesignTokens.radiusXl
                    elevated: true
                    shadowEnabled: true
                    opacity: createModalHost.modalProgress
                    scale: 0.95 + 0.05 * createModalHost.modalProgress
                    transform: Translate { y: (1.0 - createModalHost.modalProgress) * 12 }

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

                            VirelaButton {
                                compact: true
                                text: qsTr("Cancel")
                                onClicked: win.createPlaylistOpen = false
                            }

                            VirelaButton {
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
        z: 55
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
        z: 38
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: win.stageMode === "immersive" ? 22 : (DesignTokens.dockHeight + DesignTokens.dockMarginBottom + 14)
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
