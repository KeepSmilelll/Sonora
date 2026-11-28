import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
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

    property string activeSection: "online"
    property string activeLocalTab: "tracks" // "tracks" | "albums" | "artists" | "folders"
    property int activePlaylistId: -1
    property string activePlaylistName: ""
    property string unifiedSearchText: ""
    property var quickLocalResults: ({ totalTracks: 0, tracks: [], albums: [], artists: [] })
    property string detectedPlaylistId: ""

    // Continuous Spatial Stage state ("collapsed" | "rail" | "immersive")
    property string stageMode: "collapsed"
    property string previousNonImmersiveMode: "collapsed"
    property string stageTab: "lyrics" // "lyrics" | "queue"
    property bool railExpanded: true
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
    property string pendingDialogName: ""

    function openAppDialog(name) {
        if (dialogsLoader.item) {
            dialogsLoader.item.openByName(name)
        } else {
            pendingDialogName = name
            dialogsLoader.active = true
        }
    }

    function syncTitleBarDragState() {
        const allowDrag = !win.createPlaylistOpen && win.inspectedTrackMap === null
        platformBridge.setTitleBarDragEnabled(allowDrag)
    }

    function dismissSearchIfOutside(wx, wy) {
        if (titleBar.searchFocused
            && !titleBar.isPointInsideSearch(wx, wy)
            && !(searchPopoverLoader.item && searchPopoverLoader.item.isPointInside(wx, wy))) {
            titleBar.clearSearchFocus()
        }
    }

    function dismissSearchFocus() {
        titleBar.clearSearchFocus()
    }

    function refreshQuickSuggestions() {
        const q = win.unifiedSearchText.trim()
        if (q.length === 0) {
            win.quickLocalResults = ({ totalTracks: 0, tracks: [], albums: [], artists: [] })
            win.detectedPlaylistId = ""
            return
        }
        win.quickLocalResults = library.quickSearchLocal(q, 5, 3, 2)
        win.detectedPlaylistId = onlineMusic.detectPlaylistLinkId(q)
    }

    Timer {
        id: quickSearchDebounce
        interval: 90
        repeat: false
        onTriggered: win.refreshQuickSuggestions()
    }

    function openSection(section) {
        const changed = win.activeSection !== section || win.activePlaylistId !== -1
        win.activeSection = section
        win.activePlaylistId = -1
        library.releaseInactiveModels(section)
        memoryTrimTimer.restart()
        if (section === "local") {
            library.loadSection(win.activeLocalTab === "folders" ? "folders" : "tracks")
        } else if (section === "favorites" || section === "recent") {
            library.loadSection(section)
        }
        if (changed) win.triggerViewTransition()
    }

    function computeRailCoverGeometry(stageW) {
        const art = Math.round(Math.max(124, Math.min(196, stageW - 76)))
        return { x: Math.round((stageW - art) * 0.5), y: 64, width: art, height: art, radius: art * 0.5 }
    }

    function computeImmersiveCoverGeometry(stageW, stageH, vizEnabled) {
        const bodyW = Math.max(300, stageW - 32)
        const bodyH = Math.max(240, stageH - 72)
        const leftColW = Math.round(Math.max(296, Math.min(430, bodyW * 0.39)))
        const controlsH = 66 + 36 + (vizEnabled ? 44 : 0) + 46 + 52 + 42
        const art = Math.round(Math.max(168, Math.min(308, leftColW - 56, bodyH - controlsH - 24)))
        const groupH = art + controlsH
        const groupY = Math.max(6, Math.round((bodyH - groupH) * 0.45))
        return {
            leftColW: leftColW,
            groupH: groupH,
            groupY: groupY,
            x: 16 + Math.round((leftColW - art) * 0.5),
            y: 56 + groupY,
            width: art,
            height: art,
            radius: art * 0.5
        }
    }

    Timer {
        id: memoryTrimTimer
        interval: 1200
        repeat: false
        onTriggered: {
            themeManager.trimTransientMemory()
            onlineMusic.trimMemory()
        }
    }

    Connections {
        target: onlineMusic
        function onDetailChanged() {
            if (!onlineMusic.inDetail) {
                memoryTrimTimer.restart()
            }
        }
    }

    function openLocalTab(tab) {
        const valid = (tab === "albums" || tab === "artists" || tab === "folders") ? tab : "tracks"
        win.activeLocalTab = valid
        win.openSection("local")
        library.loadSection(valid === "folders" ? "folders" : "tracks")
    }

    function submitUnifiedSearch(text) {
        const q = text.trim()
        win.unifiedSearchText = q
        if (q.length === 0) {
            library.searchQuery = ""
            onlineMusic.search("", onlineMusic.searchType)
            win.quickLocalResults = ({ totalTracks: 0, tracks: [], albums: [], artists: [] })
            win.detectedPlaylistId = ""
            titleBar.clearSearchFocus()
            return
        }
        const pid = onlineMusic.detectPlaylistLinkId(q)
        if (pid.length > 0 && onlineMusic.backendConfigured) {
            win.openSection("online")
            onlineMusic.parseAndOpenPlaylist(q)
            titleBar.clearSearchFocus()
            return
        }
        if (onlineMusic.backendConfigured) {
            win.openSection("online")
            const category = appSettings.onlineNeteaseApiUrl.length > 0 ? onlineMusic.searchType : "songs"
            onlineMusic.search(q, category)
            win.quickLocalResults = library.quickSearchLocal(q, 5, 3, 2)
            titleBar.clearSearchFocus()
        } else {
            library.searchQuery = q
            win.openLocalTab("tracks")
            titleBar.clearSearchFocus()
        }
    }

    function openOnlineSettings() {
        win.openSection("settings")
        Qt.callLater(() => {
            if (settingsLoader.item) settingsLoader.item.showOnlineSources()
        })
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
                const miniComp = Qt.createComponent(Qt.resolvedUrl("windows/MiniPlayerWindow.qml"))
                if (miniComp && miniComp.status === Component.Ready) {
                    miniWindowObj = miniComp.createObject(null, {
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
        sequence: "Ctrl+K"
        enabled: win.stageMode !== "immersive" && !win.createPlaylistOpen && win.inspectedTrackMap === null
        onActivated: titleBar.focusSearch()
    }

    Shortcut {
        sequence: "Escape"
        enabled: titleBar.searchFocused || win.createPlaylistOpen || win.inspectedTrackMap !== null || win.eqOpen || win.stageMode === "immersive"
        onActivated: {
            if (titleBar.searchFocused) titleBar.clearSearchFocus()
            else if (win.createPlaylistOpen) win.createPlaylistOpen = false
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

    // Lazy-loaded Native OS Dialogs container (QtQuick.Dialogs not loaded at startup)
    Loader {
        id: dialogsLoader
        active: false
        onActiveChanged: {
            if (active) {
                setSource(Qt.resolvedUrl("components/AppDialogs.qml"), {
                    playerCtrl: player,
                    libCtrl: library,
                    onlineCtrl: onlineMusic,
                    exportPlaylistTargetId: win.exportPlaylistTargetId
                })
            } else {
                source = ""
            }
        }
        onLoaded: {
            if (win.pendingDialogName.length > 0) {
                const dName = win.pendingDialogName
                win.pendingDialogName = ""
                item.openByName(dName)
            }
        }
    }
    Binding {
        target: dialogsLoader.item
        property: "exportPlaylistTargetId"
        value: win.exportPlaylistTargetId
        when: dialogsLoader.item !== null
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
        searchText: win.unifiedSearchText
        isMaximized: win.visibility === Window.Maximized
        railExpanded: win.railExpanded
        stageMode: win.effectiveStageMode
        immersiveFactor: win.animImmersive
        onToggleRailExpanded: win.railExpanded = !win.railExpanded
        onToggleStagePanel: win.toggleStagePanel()
        onSearchEdited: (text) => {
            win.unifiedSearchText = text
            quickSearchDebounce.restart()
        }
        onSearchSubmitted: (text) => win.submitUnifiedSearch(text)
        onSearchCleared: {
            win.unifiedSearchText = ""
            win.quickLocalResults = ({ totalTracks: 0, tracks: [], albums: [], artists: [] })
            win.detectedPlaylistId = ""
            if (win.activeSection === "local") {
                library.searchQuery = ""
            } else if (win.activeSection === "online") {
                onlineMusic.search("", onlineMusic.searchType)
            }
        }
        onOpenFilesRequested: win.openAppDialog("openAudio")
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
                    pinnedPlaylists: library.pinnedPlaylists
                    activePlaylistId: win.activePlaylistId
                    activePinnedRemoteId: onlineMusic.inDetail && onlineMusic.detailType === "playlist" ? onlineMusic.detailRemoteId : ""
                    activeDownloadCount: onlineMusic.activeDownloadCount
                    onToggleExpandedRequested: win.railExpanded = !win.railExpanded
                    onSectionSelected: (section) => {
                        if (section === "online") {
                            onlineMusic.closeDetail()
                        }
                        win.openSection(section)
                    }
                    onPlaylistSelected: (plId, plName) => {
                        const changed = (win.activeSection !== "playlist" || win.activePlaylistId !== plId)
                        win.activeSection = "playlist"
                        win.activePlaylistId = plId
                        win.activePlaylistName = plName
                        library.releaseInactiveModels("playlist")
                        library.openPlaylist(plId)
                        if (changed) win.triggerViewTransition()
                    }
                    onPinnedPlaylistSelected: (src, remoteId, name, creator, coverUrl) => {
                        win.openSection("online")
                        onlineMusic.openPlaylist(remoteId, name, creator, coverUrl)
                    }
                    onUnpinOnlinePlaylistRequested: (src, remoteId) => library.unpinOnlinePlaylist(src, remoteId)
                    onCreatePlaylistRequested: win.createPlaylistOpen = true
                    onImportM3uRequested: win.openAppDialog("importM3u")
                }

                Rectangle {
                    id: contentStage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: DesignTokens.radiusXl
                    readonly property bool tracksVisible: win.activeSection === "favorites"
                        || win.activeSection === "recent"
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
                            id: tracksLoader
                            anchors.fill: parent
                            active: win.activeSection === "favorites"
                                 || win.activeSection === "recent"
                                 || win.activeSection === "playlist"
                            visible: active
                            function syncSource() {
                                if (active) {
                                    setSource(Qt.resolvedUrl("views/TracksView.qml"), {
                                        controller: player,
                                        lib: library,
                                        sectionKey: win.activeSection,
                                        activePlaylistId: win.activePlaylistId,
                                        sectionTitle: win.activeSection === "favorites"
                                            ? qsTr("Favorites")
                                            : (win.activeSection === "recent"
                                                ? qsTr("Recently Played")
                                                : (win.activeSection === "playlist"
                                                    ? win.activePlaylistName
                                                    : qsTr("All Tracks")))
                                    })
                                } else {
                                    source = ""
                                }
                            }
                            onActiveChanged: syncSource()
                            Component.onCompleted: if (active) syncSource()
                            Binding {
                                target: tracksLoader.item
                                property: "sectionKey"
                                value: win.activeSection
                                when: tracksLoader.item !== null
                            }
                            Binding {
                                target: tracksLoader.item
                                property: "activePlaylistId"
                                value: win.activePlaylistId
                                when: tracksLoader.item !== null
                            }
                            Binding {
                                target: tracksLoader.item
                                property: "sectionTitle"
                                value: win.activeSection === "favorites"
                                    ? qsTr("Favorites")
                                    : (win.activeSection === "recent"
                                        ? qsTr("Recently Played")
                                        : (win.activeSection === "playlist"
                                            ? win.activePlaylistName
                                            : qsTr("All Tracks")))
                                when: tracksLoader.item !== null
                            }
                            Connections {
                                target: tracksLoader.item
                                function onAddFolderRequested() { win.openAppDialog("addFolder") }
                                function onOpenFilesRequested() { win.openAppDialog("openAudio") }
                                function onExportPlaylistRequested(plId) {
                                    win.exportPlaylistTargetId = plId
                                    win.openAppDialog("exportM3u")
                                }
                                function onDeletePlaylistRequested(plId) {
                                    library.deletePlaylist(plId)
                                    win.openSection("local")
                                }
                                function onInspectTrack(map) { win.inspectedTrackMap = map }
                            }
                        }

                        Loader {
                            id: localHubLoader
                            anchors.fill: parent
                            active: win.activeSection === "local"
                            visible: active
                            function syncSource() {
                                if (active) {
                                    setSource(Qt.resolvedUrl("views/LocalMusicHubView.qml"), {
                                        controller: player,
                                        lib: library,
                                        heroLayer: heroLayer,
                                        activeTab: win.activeLocalTab
                                    })
                                } else {
                                    source = ""
                                }
                            }
                            onActiveChanged: syncSource()
                            Component.onCompleted: if (active) syncSource()
                            Binding {
                                target: localHubLoader.item
                                property: "activeTab"
                                value: win.activeLocalTab
                                when: localHubLoader.item !== null
                            }
                            Connections {
                                target: localHubLoader.item
                                function onTabChanged(tab) {
                                    win.activeLocalTab = tab
                                    library.loadSection(tab === "folders" ? "folders" : "tracks")
                                }
                                function onAddFolderRequested() { win.openAppDialog("addFolder") }
                                function onOpenFilesRequested() { win.openAppDialog("openAudio") }
                                function onInspectTrack(map) { win.inspectedTrackMap = map }
                            }
                        }

                        Loader {
                            id: downloadsLoader
                            anchors.fill: parent
                            active: win.activeSection === "downloads"
                            visible: active
                            function syncSource() {
                                if (active) {
                                    setSource(Qt.resolvedUrl("views/DownloadsView.qml"), {
                                        controller: player,
                                        onlineCtrl: onlineMusic,
                                        lib: library,
                                        settingsObj: appSettings
                                    })
                                } else {
                                    source = ""
                                }
                            }
                            onActiveChanged: syncSource()
                            Component.onCompleted: if (active) syncSource()
                            Connections {
                                target: downloadsLoader.item
                                function onOpenSettingsRequested() { win.openOnlineSettings() }
                                function onInspectTrack(map) { win.inspectedTrackMap = map }
                            }
                        }

                        Loader {
                            id: onlineLoader
                            anchors.fill: parent
                            active: win.activeSection === "online"
                            visible: active
                            function syncSource() {
                                if (active) {
                                    setSource(Qt.resolvedUrl("views/OnlineMusicView.qml"), {
                                        controller: player,
                                        onlineCtrl: onlineMusic,
                                        lib: library,
                                        settingsObj: appSettings,
                                        neteaseConfigured: appSettings.onlineNeteaseApiUrl.length > 0,
                                        localMatchCount: win.quickLocalResults.totalTracks || 0
                                    })
                                } else {
                                    source = ""
                                }
                            }
                            onActiveChanged: syncSource()
                            Component.onCompleted: if (active) syncSource()
                            Binding {
                                target: onlineLoader.item
                                property: "neteaseConfigured"
                                value: appSettings.onlineNeteaseApiUrl.length > 0
                                when: onlineLoader.item !== null
                            }
                            Binding {
                                target: onlineLoader.item
                                property: "localMatchCount"
                                value: win.quickLocalResults.totalTracks || 0
                                when: onlineLoader.item !== null
                            }
                            Connections {
                                target: onlineLoader.item
                                function onOpenSettingsRequested() { win.openOnlineSettings() }
                                function onOpenLocalRequested() { win.openLocalTab("tracks") }
                                function onOpenRecentRequested() { win.openSection("recent") }
                                function onOpenLocalAlbumRequested(albumName, artistName, coverHash) {
                                    win.openLocalTab("albums")
                                    Qt.callLater(() => {
                                        if (localHubLoader.item) {
                                            localHubLoader.item.openAlbumDetail(albumName, artistName, coverHash, 0)
                                        }
                                    })
                                }
                                function onViewLocalMatchesRequested(q) {
                                    library.searchQuery = q
                                    win.openLocalTab("tracks")
                                }
                                function onFocusSearchRequested() { titleBar.focusSearch() }
                                function onInspectTrack(map) { win.inspectedTrackMap = map }
                            }
                        }

                        Loader {
                            id: settingsLoader
                            anchors.fill: parent
                            active: win.activeSection === "settings"
                            visible: active
                            onActiveChanged: {
                                if (active) {
                                    setSource(Qt.resolvedUrl("views/SettingsView.qml"), {
                                        controller: player,
                                        lib: library,
                                        settingsObj: appSettings,
                                        themeObj: themeManager,
                                        onlineCtrl: onlineMusic
                                    })
                                } else {
                                    source = ""
                                }
                            }
                            Connections {
                                target: settingsLoader.item
                                function onAddFolderRequested() { win.openAppDialog("addFolder") }
                                function onSelectDownloadDirRequested() { win.openAppDialog("selectDownloadDir") }
                                function onSelectNeteaseServiceDirRequested() { win.openAppDialog("selectNeteaseServiceDir") }
                                function onSelectMetingServiceDirRequested() { win.openAppDialog("selectMetingServiceDir") }
                            }
                        }
                    }
                }
            }
        }
    }

    // Continuous Focus Stage (Collapsed <-> Companion Rail <-> 100% Full-Window Immersive, lazy-loaded when expanded)
    Item {
        id: focusStage
        anchors.top: parent.top
        anchors.topMargin: Math.round((DesignTokens.titleBarHeight + 10) * (1.0 - win.animImmersive))
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round((DesignTokens.dockHeight + DesignTokens.dockMarginBottom + 14) * (1.0 - win.animImmersive))
        anchors.right: parent.right
        anchors.rightMargin: Math.round(16 * (1.0 - win.animImmersive))
        z: win.animImmersive > 0.02 ? 32 : 15

        readonly property real liveBassEnergy: focusStageLoader.item ? focusStageLoader.item.liveBassEnergy : 0.0
        readonly property real sharedVinylAngle: focusStageLoader.item ? focusStageLoader.item.sharedVinylAngle : 0.0
        readonly property real immersiveLeftColWidth: focusStageLoader.item ? focusStageLoader.item.immersiveLeftColWidth : 340
        readonly property bool draggingResize: focusStageLoader.item ? focusStageLoader.item.draggingResize : false

        function currentCoverItem() {
            return focusStageLoader.item ? focusStageLoader.item.currentCoverItem() : null
        }
        function currentCoverRadius() {
            return focusStageLoader.item ? focusStageLoader.item.currentCoverRadius() : 23
        }
        function computeRailCoverGeometry(stageW) {
            return win.computeRailCoverGeometry(stageW)
        }
        function computeImmersiveCoverGeometry(stageW, stageH, vizEnabled) {
            return win.computeImmersiveCoverGeometry(stageW, stageH, vizEnabled)
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

        Loader {
            id: focusStageLoader
            anchors.fill: parent
            active: win.effectiveStageMode !== "collapsed" || focusStage.width > 2
            onActiveChanged: {
                if (active) {
                    setSource(Qt.resolvedUrl("views/FocusStage.qml"), {
                        controller: player,
                        lyricsCtrl: lyricsController,
                        settingsObj: appSettings,
                        stageMode: win.effectiveStageMode,
                        immersiveFactor: win.animImmersive,
                        activeTab: win.stageTab,
                        eqOpen: win.eqOpen,
                        currentDownloadStatus: onlineMusic.currentTrackDownloadStatus,
                        hideCoverForHero: heroLayer.isConcealing("nowplaying-cover")
                    })
                } else {
                    source = ""
                    memoryTrimTimer.restart()
                }
            }
            Binding { target: focusStageLoader.item; property: "stageMode"; value: win.effectiveStageMode; when: focusStageLoader.item !== null }
            Binding { target: focusStageLoader.item; property: "immersiveFactor"; value: win.animImmersive; when: focusStageLoader.item !== null }
            Binding { target: focusStageLoader.item; property: "activeTab"; value: win.stageTab; when: focusStageLoader.item !== null }
            Binding { target: focusStageLoader.item; property: "eqOpen"; value: win.eqOpen; when: focusStageLoader.item !== null }
            Binding { target: focusStageLoader.item; property: "currentDownloadStatus"; value: onlineMusic.currentTrackDownloadStatus; when: focusStageLoader.item !== null }
            Binding { target: focusStageLoader.item; property: "hideCoverForHero"; value: heroLayer.isConcealing("nowplaying-cover"); when: focusStageLoader.item !== null }
            Connections {
                target: focusStageLoader.item
                function onActiveTabChanged() {
                    if (focusStageLoader.item && win.stageTab !== focusStageLoader.item.activeTab) {
                        win.stageTab = focusStageLoader.item.activeTab
                    }
                }
                function onRequestImmersiveToggle() { win.toggleImmersive(focusStage.currentCoverItem()) }
                function onRequestCollapse() { win.stageMode = "collapsed" }
                function onToggleFavorite() { win.toggleCurrentFavorite() }
                function onToggleEq() { win.eqOpen = !win.eqOpen }
                function onOpenTrackInfo() { win.inspectedTrackMap = player.currentTrackMap }
                function onDownloadCurrentRequested() { onlineMusic.downloadCurrentTrack() }
                function onDragResize(deltaX) {
                    win.stageRailWidth = Math.max(120, Math.min(workspaceHost.width * 0.72, win.stageRailWidth - deltaX))
                }
                function onDragResizeFinished(currentW) {
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
            onActiveChanged: {
                if (active) {
                    setSource(Qt.resolvedUrl("views/EqDrawer.qml"), {
                        controller: player,
                        settingsObj: appSettings
                    })
                } else {
                    source = ""
                }
            }
            Connections {
                target: eqLoader.item
                function onCloseRequested() { win.eqOpen = false }
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
        currentDownloadStatus: onlineMusic.currentTrackDownloadStatus
        hideCoverForHero: heroLayer.isConcealing("nowplaying-cover")
        onToggleQueue: win.toggleStageQueue()
        onToggleLyrics: win.toggleStageLyrics(playbackDock.coverAnchorItem())
        onCoverClicked: win.toggleImmersive(playbackDock.coverAnchorItem())
        onToggleEq: win.eqOpen = !win.eqOpen
        onOpenTrackInfo: win.inspectedTrackMap = player.currentTrackMap
        onDownloadCurrentRequested: onlineMusic.downloadCurrentTrack()
        onToggleFavorite: win.toggleCurrentFavorite()
    }

    function toggleCurrentFavorite() {
        if (player.hasTrack) {
            library.toggleTrackFavorite(player.currentTrackMap)
        }
    }

    // Unified Command-Search Suggestions Popover (lazy-loaded on search focus)
    Loader {
        id: searchPopoverLoader
        anchors.fill: parent
        z: 36
        active: titleBar.searchFocused && win.stageMode !== "immersive"
        onActiveChanged: {
            if (active) {
                setSource(Qt.resolvedUrl("components/SearchSuggestionsPopover.qml"), {
                    open: true,
                    query: win.unifiedSearchText,
                    onlineConfigured: onlineMusic.backendConfigured,
                    playlistLinkId: win.detectedPlaylistId,
                    localResults: win.quickLocalResults,
                    anchorRect: titleBar.searchPillRectInWindow()
                })
            } else {
                source = ""
            }
        }
        Binding { target: searchPopoverLoader.item; property: "query"; value: win.unifiedSearchText; when: searchPopoverLoader.item !== null }
        Binding { target: searchPopoverLoader.item; property: "onlineConfigured"; value: onlineMusic.backendConfigured; when: searchPopoverLoader.item !== null }
        Binding { target: searchPopoverLoader.item; property: "playlistLinkId"; value: win.detectedPlaylistId; when: searchPopoverLoader.item !== null }
        Binding { target: searchPopoverLoader.item; property: "localResults"; value: win.quickLocalResults; when: searchPopoverLoader.item !== null }
        Binding { target: searchPopoverLoader.item; property: "anchorRect"; value: titleBar.searchPillRectInWindow(); when: searchPopoverLoader.item !== null }
        Connections {
            target: searchPopoverLoader.item
            function onOpenPlaylistLinkRequested(rawInput) { win.submitUnifiedSearch(rawInput) }
            function onSearchOnlineRequested(q) { win.submitUnifiedSearch(q) }
            function onOpenOnlineSettingsRequested() {
                titleBar.clearSearchFocus()
                win.openOnlineSettings()
            }
            function onPlayLocalTrackRequested(trackId) {
                library.playTrackById(trackId)
                titleBar.clearSearchFocus()
            }
            function onOpenLocalAlbumRequested(album, albumArtist, coverHash, year) {
                titleBar.clearSearchFocus()
                win.openLocalTab("albums")
                Qt.callLater(() => {
                    if (localHubLoader.item) {
                        localHubLoader.item.openAlbumDetail(album, albumArtist, coverHash, year)
                    }
                })
            }
            function onOpenLocalArtistRequested(artist) {
                titleBar.clearSearchFocus()
                win.openLocalTab("artists")
                Qt.callLater(() => {
                    if (localHubLoader.item) {
                        localHubLoader.item.selectArtist(artist)
                    }
                })
            }
            function onFilterLocalLibraryRequested(q) {
                library.searchQuery = q
                titleBar.clearSearchFocus()
                win.openLocalTab("tracks")
            }
        }
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
            onActiveChanged: {
                if (active) {
                    setSource(Qt.resolvedUrl("components/TrackInfoDialog.qml"), {
                        open: win.inspectedTrackMap !== null,
                        trackMap: win.inspectedTrackMap || win.cachedTrackMap
                    })
                } else {
                    source = ""
                }
            }
            Binding { target: trackInfoLoader.item; property: "open"; value: win.inspectedTrackMap !== null; when: trackInfoLoader.item !== null }
            Binding { target: trackInfoLoader.item; property: "trackMap"; value: win.inspectedTrackMap || win.cachedTrackMap; when: trackInfoLoader.item !== null }
            Connections {
                target: trackInfoLoader.item
                function onCloseRequested() { win.inspectedTrackMap = null }
                function onClosed() {
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
