import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Virela.UI

// Continuous Focus Stage: seamlessly morphs between Rail (side companion), Collapsed (0px),
// and Immersive (100% borderless fluid stage), unifying Now Playing lyrics and Queue.
Item {
    id: root

    required property var controller
    required property var lyricsCtrl
    required property var settingsObj

    property string stageMode: "rail" // "collapsed" | "rail" | "immersive"
    property real immersiveFactor: 0.0
    property string activeTab: "lyrics" // "lyrics" | "queue"
    property bool hideCoverForHero: false
    property bool draggingResize: false
    property bool eqOpen: false
    property int trackSwitchDirection: 1 // 1 = forward, -1 = backward

    readonly property real immersiveLeftColWidth: immersiveView.immGeom.leftColW
    readonly property real liveBassEnergy: (root.controller.isPlaying
        && root.settingsObj && root.settingsObj.visualizerEnabled
        && root.stageMode !== "collapsed")
        ? (root.immersiveFactor >= 0.5 ? immSpectrum.bassEnergy : railSpectrum.bassEnergy)
        : 0.0

    signal requestImmersiveToggle()
    signal requestCollapse()
    signal toggleFavorite()
    signal toggleEq()
    signal openTrackInfo()
    signal dragResize(real deltaX)
    signal dragResizeFinished(real currentWidth)

    clip: true

    // Shared master LP vinyl rotation across Rail & Immersive views (0 idle CPU when paused/hidden)
    property real sharedVinylAngle: 0.0
    property real _vinylSpeed: 0.0
    readonly property real _targetVinylSpeed: root.controller.isPlaying ? 24.0 : 0.0

    FrameAnimation {
        running: root.visible && (root.controller.isPlaying || root._vinylSpeed > 0.15)
        onTriggered: {
            const dt = Math.min(0.05, frameTime)
            const accel = root.controller.isPlaying ? 4.8 : 2.8
            root._vinylSpeed += (root._targetVinylSpeed - root._vinylSpeed) * Math.min(1.0, dt * accel)
            if (!root.controller.isPlaying && root._vinylSpeed < 0.15) {
                root._vinylSpeed = 0.0
                return
            }
            root.sharedVinylAngle = (root.sharedVinylAngle + root._vinylSpeed * dt) % 360.0
        }
    }

    // Zero-lag 60fps audio-clock interpolation for true word-timed karaoke lyrics
    // (0 idle CPU on ordinary line-level LRC or when paused)
    property real liveEffectivePositionMs: 0.0
    property real _clockAnchorPosMs: 0.0
    property double _clockAnchorWallMs: 0.0

    FrameAnimation {
        running: root.visible
            && root.stageMode !== "collapsed"
            && root.activeTab === "lyrics"
            && root.controller.isPlaying
            && root.lyricsCtrl.hasEnhancedWords
        onTriggered: {
            const elapsed = Math.max(0.0, Math.min(80.0, Date.now() - root._clockAnchorWallMs))
            root.liveEffectivePositionMs = root._clockAnchorPosMs + elapsed
        }
    }

    Connections {
        target: root.lyricsCtrl
        function onPositionProgressChanged() {
            const target = root.lyricsCtrl.effectivePositionMs
            root._clockAnchorPosMs = target
            root._clockAnchorWallMs = Date.now()
            if (!root.controller.isPlaying || !root.lyricsCtrl.hasEnhancedWords
                || Math.abs(target - root.liveEffectivePositionMs) > 90) {
                root.liveEffectivePositionMs = target
            } else if (target > root.liveEffectivePositionMs) {
                root.liveEffectivePositionMs = target
            }
        }
        function onLyricsChanged() {
            const target = root.lyricsCtrl.effectivePositionMs
            root._clockAnchorPosMs = target
            root._clockAnchorWallMs = Date.now()
            root.liveEffectivePositionMs = target
        }
    }

    // Deterministic closed-form circular vinyl geometry shared by QML items & HeroTransitionLayer
    function computeRailCoverGeometry(stageW) {
        const art = Math.round(Math.max(124, Math.min(196, stageW - 76)))
        return {
            x: Math.round((stageW - art) * 0.5),
            y: 64,
            width: art,
            height: art,
            radius: art * 0.5
        }
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

    component ImmIconBtn: Item {
        id: btn
        property string iconName: "play"
        property bool active: false
        property bool showActiveDot: false
        property real iconSize: 16
        signal clicked()

        implicitWidth: 30
        implicitHeight: 30

        scale: btnMa.pressed ? 0.89 : (btnMa.containsMouse ? 1.07 : 1.0)
        Behavior on scale {
            NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.15 }
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: {
                if (btnMa.pressed) return DesignTokens.surfacePressed
                if (btnMa.containsMouse) return DesignTokens.surfaceHover
                return "transparent"
            }
            Behavior on color { ColorAnimation { duration: 130 } }
        }

        IconGlyph {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: (btn.showActiveDot && btn.active) ? -2 : 0
            name: btn.iconName
            size: btn.iconSize
            color: btn.active
                ? DesignTokens.accentLight
                : (btnMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
        }

        Rectangle {
            visible: btn.showActiveDot
            width: 4
            height: 4
            radius: 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            color: DesignTokens.accent
            opacity: btn.active ? 1.0 : 0.0
            scale: btn.active ? 1.0 : 0.3
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
        }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    function syncImmersiveHeaderRegions() {
        if (root.immersiveFactor < 0.5 || typeof platformBridge === "undefined" || !platformBridge) return
        const rects = []
        const addRect = function(item) {
            if (item && item.visible && item.width > 0 && item.height > 0) {
                const p = item.mapToItem(null, 0, 0)
                rects.push({ x: p.x, y: p.y, width: item.width, height: item.height })
            }
        }
        addRect(tabPill)
        addRect(clearQueueBtn)
        addRect(expToggleBtn)
        platformBridge.setTitleBarInteractiveRects(rects)
    }

    onImmersiveFactorChanged: if (immersiveFactor >= 0.5) Qt.callLater(syncImmersiveHeaderRegions)
    onWidthChanged: if (immersiveFactor >= 0.5) Qt.callLater(syncImmersiveHeaderRegions)
    onActiveTabChanged: if (immersiveFactor >= 0.5) Qt.callLater(syncImmersiveHeaderRegions)

    function predictedCoverRectInWorkspace(workspaceW, workspaceH, targetMode, railTargetW) {
        const vizOn = Boolean(root.settingsObj && root.settingsObj.visualizerEnabled)
        if (targetMode === "immersive") {
            const g = computeImmersiveCoverGeometry(workspaceW, workspaceH, vizOn)
            return { x: g.x, y: g.y, width: g.width, height: g.height, radius: g.radius }
        }
        const stageW = Math.min(railTargetW, Math.max(DesignTokens.stageMinWidth, workspaceW * 0.42))
        const rg = computeRailCoverGeometry(stageW)
        return {
            x: Math.round(workspaceW - stageW + rg.x),
            y: rg.y,
            width: rg.width,
            height: rg.height,
            radius: rg.radius
        }
    }

    function currentCoverItem() {
        return root.immersiveFactor >= 0.5 ? immersiveCoverBox : railCoverBox
    }

    function currentCoverRadius() {
        return (root.immersiveFactor >= 0.5 ? immersiveCoverBox.width : railCoverBox.width) * 0.5
    }

    // Track switch directional micro-parallax
    property string lastTrackIdentity: ""
    property real artSwitchShift: 0.0
    property real artSwitchOpacity: 1.0

    Connections {
        target: root.controller
        function onCurrentTrackChanged() {
            const idKey = root.controller.trackFilePath + "|" + root.controller.trackTitle
            if (root.lastTrackIdentity.length > 0 && idKey !== root.lastTrackIdentity && root.controller.hasTrack) {
                artSwitchAnim.restart()
            }
            root.lastTrackIdentity = idKey
        }
        function onIsPlayingChanged() {
            const target = root.lyricsCtrl.effectivePositionMs
            root._clockAnchorPosMs = target
            root._clockAnchorWallMs = Date.now()
            root.liveEffectivePositionMs = target
        }
    }

    ParallelAnimation {
        id: artSwitchAnim
        NumberAnimation {
            target: root
            property: "artSwitchShift"
            from: 18 * root.trackSwitchDirection
            to: 0
            duration: 280
            easing.type: Easing.OutQuint
        }
        NumberAnimation {
            target: root
            property: "artSwitchOpacity"
            from: 0.45
            to: 1.0
            duration: 240
            easing.type: Easing.OutCubic
        }
    }

    // L1 Stage background smoothly dissolves into completely borderless fluid aurora when immersive
    Rectangle {
        anchors.fill: parent
        radius: DesignTokens.radiusLg * (1.0 - root.immersiveFactor)
        color: {
            const c = DesignTokens.stageSurfaceBg
            return Qt.rgba(c.r, c.g, c.b, c.a * (1.0 - root.immersiveFactor))
        }
        border.width: root.immersiveFactor < 0.98 ? 1 : 0
        border.color: Qt.rgba(DesignTokens.hairlineBorder.r,
                              DesignTokens.hairlineBorder.g,
                              DesignTokens.hairlineBorder.b,
                              DesignTokens.hairlineBorder.a * (1.0 - root.immersiveFactor))
    }

    // Left-edge drag resize handle (active in Rail mode)
    Item {
        id: resizeEdgeHandle
        visible: root.stageMode === "rail" && root.width > 40
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 9
        z: 30

        Rectangle {
            anchors.centerIn: parent
            width: 3
            height: resizeMa.containsMouse || root.draggingResize ? 46 : 24
            radius: 1.5
            color: resizeMa.containsMouse || root.draggingResize ? DesignTokens.accent : DesignTokens.borderHighlight
            opacity: resizeMa.containsMouse || root.draggingResize ? 0.95 : 0.28
            Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        MouseArea {
            id: resizeMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property real pressGlobalX: 0

            onPressed: (mouse) => {
                root.draggingResize = true
                pressGlobalX = mapToItem(null, mouse.x, 0).x
            }
            onPositionChanged: (mouse) => {
                if (!pressed) return
                const curX = mapToItem(null, mouse.x, 0).x
                const dx = curX - pressGlobalX
                pressGlobalX = curX
                root.dragResize(dx)
            }
            onReleased: {
                root.draggingResize = false
                root.dragResizeFinished(root.width)
            }
            onCanceled: {
                root.draggingResize = false
            }
        }
    }

    // Top Stage Header (Segmented Lyrics/Queue pill + Offset controls + Immersive/Collapse)
    RowLayout {
        id: headerBar
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 36
        spacing: 8
        z: 10

        // Segmented Lyrics / Queue pill switcher
        Rectangle {
            id: tabPill
            implicitWidth: tabRow.implicitWidth + 6
            implicitHeight: 30
            onWidthChanged: if (root.immersiveFactor >= 0.5) Qt.callLater(root.syncImmersiveHeaderRegions)
            radius: 15
            color: DesignTokens.surfaceInput
            border.width: 1
            border.color: DesignTokens.hairlineBorder

            Rectangle {
                id: tabIndicator
                y: 3
                height: 24
                radius: 12
                x: root.activeTab === "lyrics" ? 3 : (3 + lyricsTabBtn.width + 2)
                width: root.activeTab === "lyrics" ? lyricsTabBtn.width : queueTabBtn.width
                color: DesignTokens.surfaceSelected
                border.width: 1
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.34)

                Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutQuint } }
                Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutQuint } }
            }

            Row {
                id: tabRow
                anchors.centerIn: parent
                spacing: 2

                Item {
                    id: lyricsTabBtn
                    width: lyrLbl.implicitWidth + 20
                    height: 24
                    Text {
                        id: lyrLbl
                        anchors.centerIn: parent
                        text: qsTr("Lyrics")
                        color: root.activeTab === "lyrics" ? DesignTokens.accentLight : DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.weight: root.activeTab === "lyrics" ? Font.DemiBold : Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "lyrics"
                    }
                }

                Item {
                    id: queueTabBtn
                    width: qLblRow.implicitWidth + 18
                    height: 24
                    Row {
                        id: qLblRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: qsTr("Queue")
                            color: root.activeTab === "queue" ? DesignTokens.accentLight : DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.weight: root.activeTab === "queue" ? Font.DemiBold : Font.Medium
                        }
                        Text {
                            visible: root.controller.queueCount > 0
                            text: String(root.controller.queueCount)
                            color: root.activeTab === "queue" ? DesignTokens.accent : DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            font.features: ({ "tnum": 1 })
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = "queue"
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        FormatBadge {
            visible: root.immersiveFactor > 0.6 && root.activeTab === "lyrics" && root.lyricsCtrl.sourceLabel.length > 0
            text: root.lyricsCtrl.sourceLabel
        }

        VirelaButton {
            id: clearQueueBtn
            visible: root.activeTab === "queue" && root.controller.queueCount > 0
            compact: true
            text: qsTr("Clear")
            onClicked: root.controller.clearQueue()
        }

        // Expand / Collapse Immersive Toggle
        Rectangle {
            id: expToggleBtn
            implicitWidth: 28
            implicitHeight: 28
            radius: 8
            color: expMa.pressed ? DesignTokens.surfacePressed : (expMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
            scale: expMa.pressed ? 0.90 : (expMa.containsMouse ? 1.07 : 1.0)
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
            Behavior on color { ColorAnimation { duration: 120 } }

            IconGlyph {
                anchors.centerIn: parent
                name: root.stageMode === "immersive" ? "collapse" : "expand"
                size: 14
                color: expMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
            }

            MouseArea {
                id: expMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.requestImmersiveToggle()
            }
        }

        // Hide Stage Panel (visible in Rail mode)
        Rectangle {
            visible: root.stageMode !== "immersive"
            implicitWidth: 28
            implicitHeight: 28
            radius: 8
            color: clsMa.pressed ? DesignTokens.surfacePressed : (clsMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
            scale: clsMa.pressed ? 0.90 : 1.0
            Behavior on scale { NumberAnimation { duration: 130 } }
            Behavior on color { ColorAnimation { duration: 120 } }

            IconGlyph {
                anchors.centerIn: parent
                name: "close"
                size: 13
                color: clsMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
            }

            MouseArea {
                id: clsMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.requestCollapse()
            }
        }
    }

    // Main Content Area
    Item {
        id: bodyArea
        anchors.top: headerBar.bottom
        anchors.topMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        // =========================================================================
        // 1. RAIL MODE (Companion Side Stage)
        // =========================================================================
        Item {
            id: railView
            anchors.fill: parent
            opacity: Math.max(0.0, 1.0 - root.immersiveFactor * 1.45)
            visible: opacity > 0.01 && root.stageMode !== "immersive"

            readonly property var coverGeom: root.computeRailCoverGeometry(root.width)

            // Rail Spinning Circular Vinyl LP (deterministic x/y matching computeRailCoverGeometry)
            Item {
                id: railCoverBox
                x: Math.round((railView.width - width) * 0.5)
                y: 8
                width: railView.coverGeom.width
                height: railView.coverGeom.height
                opacity: root.hideCoverForHero ? 0.0 : root.artSwitchOpacity
                transform: Translate { x: root.artSwitchShift }

                VinylDiscItem {
                    anchors.fill: parent
                    coverUrl: root.controller.trackCoverUrl
                    playing: root.controller.isPlaying
                    discRotation: root.sharedVinylAngle
                    ownSpin: false
                    hovered: railCoverMa.containsMouse
                    scale: railCoverMa.pressed ? 0.96 : (railCoverMa.containsMouse ? 1.03 : 1.0)
                    Behavior on scale { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }

                // Hover hint to expand to Immersive Hero Stage
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    width: 28
                    height: 28
                    radius: 14
                    color: DesignTokens.isDark ? Qt.rgba(0, 0, 0, 0.68) : Qt.rgba(1, 1, 1, 0.92)
                    border.width: 1
                    border.color: DesignTokens.borderHighlight
                    opacity: railCoverMa.containsMouse && root.controller.hasTrack ? 1.0 : 0.0
                    scale: railCoverMa.containsMouse ? 1.0 : 0.8
                    Behavior on opacity { NumberAnimation { duration: 140 } }
                    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }

                    IconGlyph {
                        anchors.centerIn: parent
                        name: "expand"
                        size: 13
                        color: DesignTokens.isDark ? "#ffffff" : DesignTokens.accent
                    }
                }

                MouseArea {
                    id: railCoverMa
                    anchors.fill: parent
                    enabled: root.controller.hasTrack
                    hoverEnabled: true
                    cursorShape: root.controller.hasTrack ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.requestImmersiveToggle()
                }
            }

            // Metadata + Badges + Mini Spectrum below Rail Cover
            ColumnLayout {
                id: railMetaCol
                anchors.top: railCoverBox.bottom
                anchors.topMargin: 12
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Item { Layout.preferredWidth: 24; visible: railFavBtn.visible }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: root.controller.hasTrack ? root.controller.trackTitle : qsTr("Ready to Listen")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }

                    Item {
                        id: railFavBtn
                        visible: root.controller.hasTrack && root.controller.trackId >= 0
                        implicitWidth: 24
                        implicitHeight: 24
                        property real popScale: 1.0
                        scale: popScale

                        SequentialAnimation {
                            id: railHeartPop
                            NumberAnimation { target: railFavBtn; property: "popScale"; to: 1.35; duration: 105; easing.type: Easing.OutQuad }
                            NumberAnimation { target: railFavBtn; property: "popScale"; to: 1.0; duration: 140; easing.type: Easing.OutBack }
                        }

                        IconGlyph {
                            anchors.centerIn: parent
                            name: root.controller.trackIsFavorite ? "heart_filled" : "heart"
                            size: 14
                            color: root.controller.trackIsFavorite
                                ? DesignTokens.accent
                                : (railFavMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textTertiary)
                        }

                        MouseArea {
                            id: railFavMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                railHeartPop.restart()
                                root.toggleFavorite()
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: root.controller.hasTrack
                        ? (root.controller.trackArtist.length > 0 ? root.controller.trackArtist : qsTr("Unknown Artist"))
                        : qsTr("Drop audio files or pick a track to begin")
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 2
                    spacing: 6
                    visible: root.controller.hasTrack

                    FormatBadge {
                        compact: true
                        visible: root.controller.trackFormatBadge.length > 0
                        text: root.controller.trackFormatBadge
                    }
                    FormatBadge {
                        compact: true
                        visible: root.controller.outputStatusBadge.length > 0
                        text: root.controller.outputStatusBadge
                    }
                }

                SpectrumVisualizerItem {
                    id: railSpectrum
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    Layout.preferredWidth: Math.min(200, railView.width - 32)
                    Layout.preferredHeight: 24
                    styleMode: "aurora"
                    visible: root.settingsObj.visualizerEnabled && root.controller.hasTrack
                    accentColor: DesignTokens.accent
                    secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                    playing: root.controller.isPlaying
                    visualizerEnabled: root.settingsObj.visualizerEnabled && railView.visible
                }
            }

            Rectangle {
                id: railDivider
                anchors.top: railMetaCol.bottom
                anchors.topMargin: 10
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: DesignTokens.hairlineBorder
            }

            // Borderless Lyrics / Queue switchable area inside Rail
            Item {
                anchors.top: railDivider.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                // Rail Lyrics View
                ListView {
                    id: railLyricsList
                    anchors.fill: parent
                    clip: true
                    spacing: 6
                    topMargin: Math.round(height * 0.28)
                    bottomMargin: Math.round(height * 0.42)
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.lyricsCtrl
                    preferredHighlightBegin: Math.round(height * 0.28)
                    preferredHighlightEnd: Math.round(height * 0.40)
                    highlightRangeMode: userScrolling ? ListView.NoHighlightRange : ListView.StrictlyEnforceRange
                    highlightMoveDuration: 300
                    currentIndex: root.lyricsCtrl.activeLineIndex >= 0 ? root.lyricsCtrl.activeLineIndex : 0
                    visible: root.activeTab === "lyrics" && root.lyricsCtrl.hasLyrics
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    property bool userScrolling: false

                    onMovementStarted: {
                        userScrolling = true
                        railResumeTimer.restart()
                    }
                    onCountChanged: Qt.callLater(function() {
                        if (count > 0) positionViewAtIndex(Math.max(0, root.lyricsCtrl.activeLineIndex), ListView.Center)
                    })

                    Timer {
                        id: railResumeTimer
                        interval: 3000
                        onTriggered: railLyricsList.userScrolling = false
                    }

                    delegate: Item {
                        id: rlRow
                        required property int index
                        required property int timestampMs
                        required property string text
                        required property string translation
                        required property bool isActive
                        required property bool hasWords
                        required property var wordTokens

                        width: railLyricsList.width - 6
                        height: rlCol.implicitHeight + 10

                        readonly property int dist: Math.abs(index - railLyricsList.currentIndex)
                        readonly property real depthOpacity: rlRow.isActive ? 1.0
                            : (rlMa.containsMouse ? 0.92 : (dist === 1 ? 0.62 : (dist === 2 ? 0.40 : 0.26)))

                        Column {
                            id: rlCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            transformOrigin: Item.Center
                            scale: rlRow.isActive ? 1.03 : 1.0
                            Behavior on scale {
                                SpringAnimation { spring: 3.8; damping: 0.44; epsilon: 0.002 }
                            }

                            // Mode-differentiated lyrics: word-by-word sweep for word-timed formats,
                            // clean whole-line focus highlight for ordinary line-level LRC.
                            Item {
                                id: rlTextHost
                                width: parent.width
                                height: rlTextHost.karaokeActive ? rlWordFlow.implicitHeight : rlBaseText.implicitHeight

                                readonly property bool karaokeActive: rlRow.isActive && rlRow.hasWords && root.lyricsCtrl.isSynced

                                Text {
                                    id: rlMeasureText
                                    visible: false
                                    text: rlRow.text.length > 0 ? rlRow.text : "· · ·"
                                    textFormat: Text.PlainText
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: rlRow.isActive ? 15 : 13
                                    font.weight: rlRow.isActive ? Font.Bold : Font.Medium
                                }

                                Flow {
                                    id: rlWordFlow
                                    visible: rlTextHost.karaokeActive
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Math.min(parent.width, Math.ceil(rlMeasureText.implicitWidth) + 2)
                                    spacing: 0

                                    Repeater {
                                        model: rlTextHost.karaokeActive ? rlRow.wordTokens : []
                                        delegate: Item {
                                            id: rlWordSlot
                                            required property var modelData
                                            width: rlWordBase.implicitWidth
                                            height: rlWordBase.implicitHeight

                                            readonly property real spanMs: Math.max(1, modelData.endMs - modelData.startMs)
                                            readonly property real fillRatio: Math.max(0.0, Math.min(1.0,
                                                (root.liveEffectivePositionMs - modelData.startMs) / spanMs))
                                            readonly property real liftOffset: (fillRatio > 0.01 && fillRatio < 0.99)
                                                ? (-1.2 * Math.sin(fillRatio * Math.PI)) : 0.0

                                            Text {
                                                id: rlWordBase
                                                y: rlWordSlot.liftOffset
                                                text: rlWordSlot.modelData.text
                                                textFormat: Text.PlainText
                                                color: DesignTokens.textSecondary
                                                opacity: 0.42
                                                font: rlMeasureText.font
                                            }

                                            Item {
                                                anchors.left: parent.left
                                                anchors.top: parent.top
                                                height: rlWordBase.height
                                                width: rlWordBase.width * rlWordSlot.fillRatio
                                                clip: true

                                                Text {
                                                    y: rlWordSlot.liftOffset
                                                    text: rlWordBase.text
                                                    textFormat: Text.PlainText
                                                    color: DesignTokens.textPrimary
                                                    font: rlMeasureText.font
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    id: rlBaseText
                                    visible: !rlTextHost.karaokeActive
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    textFormat: Text.PlainText
                                    text: rlRow.text.length > 0 ? rlRow.text : "· · ·"
                                    color: rlRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                    opacity: rlRow.isActive ? 1.0 : rlRow.depthOpacity
                                    font: rlMeasureText.font
                                    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                            }

                            Text {
                                width: parent.width
                                visible: rlRow.translation.length > 0
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                                textFormat: Text.PlainText
                                text: rlRow.translation
                                color: DesignTokens.textTertiary
                                opacity: rlRow.isActive ? 0.88 : rlRow.depthOpacity * 0.72
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: rlMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: root.lyricsCtrl.isSynced ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.lyricsCtrl.isSynced) {
                                    const ts = root.lyricsCtrl.timestampForLine(rlRow.index)
                                    if (ts >= 0) {
                                        railLyricsList.userScrolling = false
                                        root.controller.seekMs(ts)
                                    }
                                }
                            }
                        }
                    }
                }

                // Rail Empty Lyrics hint
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    visible: root.activeTab === "lyrics" && !root.lyricsCtrl.hasLyrics
                    spacing: 8

                    IconGlyph {
                        Layout.alignment: Qt.AlignHCenter
                        name: "lyrics"
                        size: 26
                        color: DesignTokens.textTertiary
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("No Lyrics Available")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                // Rail Queue View
                ListView {
                    anchors.fill: parent
                    visible: root.activeTab === "queue"
                    clip: true
                    spacing: 4
                    reuseItems: true
                    cacheBuffer: 200
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.controller.queueModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: rqRow
                        required property int index
                        required property string title
                        required property string artist
                        required property string coverHash
                        required property int durationMs
                        required property bool isCurrent

                        readonly property bool hovered: rqMa.containsMouse || rqRmMa.containsMouse
                        readonly property bool nextStartsHere: root.controller.currentIndex >= 0
                            && index === root.controller.currentIndex + 1
                        width: ListView.view ? ListView.view.width : 280
                        height: 44 + (rqGroupLbl.visible ? 20 : 0)

                        Text {
                            id: rqGroupLbl
                            visible: rqRow.isCurrent || rqRow.nextStartsHere
                            height: visible ? 20 : 0
                            verticalAlignment: Text.AlignVCenter
                            text: rqRow.isCurrent ? qsTr("Now Playing") : qsTr("Up Next")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            anchors.top: rqGroupLbl.bottom
                            anchors.bottom: parent.bottom
                            width: parent.width
                            radius: DesignTokens.radiusSm
                            color: rqRow.isCurrent ? DesignTokens.surfaceSelected
                                : (rqRow.hovered ? DesignTokens.surfaceHover : "transparent")
                            Behavior on color { ColorAnimation { duration: 110 } }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.topMargin: rqGroupLbl.height
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                radius: 7
                                color: DesignTokens.surfaceCard
                                clip: true

                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: rqCover.status !== Image.Ready
                                    name: "music"
                                    size: 14
                                    color: DesignTokens.textTertiary
                                }
                                Image {
                                    id: rqCover
                                    anchors.fill: parent
                                    source: rqRow.isCurrent ? root.controller.trackCoverUrl
                                        : (rqRow.coverHash.length > 0 ? "image://cover/" + rqRow.coverHash + "?size=96" : "")
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: rqRow.title
                                    elide: Text.ElideRight
                                    color: rqRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 12
                                    font.weight: rqRow.isCurrent ? Font.DemiBold : Font.Medium
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: rqRow.artist.length > 0 ? rqRow.artist : qsTr("Unknown Artist")
                                    elide: Text.ElideRight
                                    color: DesignTokens.textTertiary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 10
                                }
                            }

                            NowPlayingBars {
                                visible: rqRow.isCurrent && !rqRow.hovered
                                playing: root.controller.isPlaying
                                size: 13
                            }

                            Text {
                                visible: !rqRow.isCurrent && !rqRow.hovered
                                text: root.controller.formatTime(rqRow.durationMs)
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 10
                                font.features: ({ "tnum": 1 })
                            }

                            Rectangle {
                                visible: rqRow.hovered
                                implicitWidth: 24
                                implicitHeight: 24
                                radius: 6
                                color: rqRmMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "close"
                                    size: 10
                                    color: rqRmMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                }
                                MouseArea {
                                    id: rqRmMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.controller.removeFromQueue(rqRow.index)
                                }
                            }
                        }

                        MouseArea {
                            id: rqMa
                            anchors.fill: parent
                            anchors.topMargin: rqGroupLbl.height
                            anchors.rightMargin: 30
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.controller.playQueueIndex(rqRow.index)
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        visible: root.controller.queueCount === 0
                        spacing: 8
                        IconGlyph {
                            Layout.alignment: Qt.AlignHCenter
                            name: "queue"
                            size: 24
                            color: DesignTokens.textTertiary
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Queue is empty\nDrop audio files anywhere to start listening")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            lineHeight: 1.4
                        }
                    }
                }
            }
        }

        // =========================================================================
        // 2. IMMERSIVE MODE (100% Borderless Fluid Acoustic Stage)
        // =========================================================================
        Item {
            id: immersiveView
            anchors.fill: parent
            opacity: Math.max(0.0, (root.immersiveFactor - 0.18) / 0.82)
            visible: opacity > 0.01

            readonly property var immGeom: root.computeImmersiveCoverGeometry(
                root.width, root.height, Boolean(root.settingsObj && root.settingsObj.visualizerEnabled))

            // Left Column: Hero Artwork + Typography + Spectrum + Integrated Borderless Control Tower
            Item {
                id: immLeftCol
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: immersiveView.immGeom.leftColW

                readonly property real ctrlW: Math.round(Math.max(248, Math.min(316, immLeftCol.width - 40)))

                // Subtle soft ambient circular halo behind spinning vinyl LP
                Rectangle {
                    anchors.centerIn: immersiveCoverBox
                    width: immersiveCoverBox.width + 44
                    height: immersiveCoverBox.height + 44
                    radius: width * 0.5
                    color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b,
                                   DesignTokens.isDark ? 0.14 : 0.09)
                    opacity: root.controller.hasTrack && !root.hideCoverForHero ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 240 } }
                }

                // Deterministic Hero target circular vinyl container
                Item {
                    id: immersiveCoverBox
                    x: Math.round((immLeftCol.width - width) * 0.5)
                    y: immersiveView.immGeom.groupY
                    width: immersiveView.immGeom.width
                    height: immersiveView.immGeom.height
                    opacity: root.hideCoverForHero ? 0.0 : root.artSwitchOpacity
                    transform: Translate { x: root.artSwitchShift }

                    VinylDiscItem {
                        anchors.fill: parent
                        coverUrl: root.controller.trackCoverUrl
                        playing: root.controller.isPlaying
                        discRotation: root.sharedVinylAngle
                        ownSpin: false
                        hovered: immCoverMa.containsMouse
                        scale: immCoverMa.pressed ? 0.97 : (immCoverMa.containsMouse ? 1.02 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    }

                    MouseArea {
                        id: immCoverMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.requestImmersiveToggle()
                    }
                }

                // Fixed-height Metadata Block below Artwork (matches computeImmersiveCoverGeometry)
                Column {
                    id: immTitleBlock
                    anchors.top: immersiveCoverBox.bottom
                    anchors.topMargin: 14
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 52
                    spacing: 4

                    Text {
                        width: parent.width
                        height: 28
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: root.controller.hasTrack ? root.controller.trackTitle : qsTr("Ready to Listen")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }

                    Text {
                        width: parent.width
                        height: 20
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: root.controller.hasTrack
                            ? (root.controller.trackAlbum.length > 0
                                ? (root.controller.trackArtist + " — " + root.controller.trackAlbum)
                                : root.controller.trackArtist)
                            : qsTr("Drop audio files or pick a track to begin")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                }

                Row {
                    id: immBadgesRow
                    anchors.top: immTitleBlock.bottom
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 26
                    spacing: 7

                    FormatBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.controller.trackFormatBadge.length > 0
                        text: root.controller.trackFormatBadge
                    }
                    FormatBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.controller.outputStatusBadge.length > 0
                        text: root.controller.outputStatusBadge
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.controller.trackId >= 0
                        width: 26
                        height: 26
                        radius: 13
                        color: immFavMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                        IconGlyph {
                            anchors.centerIn: parent
                            name: root.controller.trackIsFavorite ? "heart_filled" : "heart"
                            size: 14
                            color: root.controller.trackIsFavorite ? DesignTokens.accent : DesignTokens.textSecondary
                        }
                        MouseArea {
                            id: immFavMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.toggleFavorite()
                        }
                    }
                }

                SpectrumVisualizerItem {
                    id: immSpectrum
                    anchors.top: immBadgesRow.bottom
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(immLeftCol.ctrlW, Math.round(immersiveCoverBox.width * 0.96))
                    height: 34
                    styleMode: "aurora"
                    visible: root.settingsObj.visualizerEnabled && root.immersiveFactor > 0.25
                    accentColor: DesignTokens.accent
                    secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                    playing: root.controller.isPlaying
                    visualizerEnabled: root.settingsObj.visualizerEnabled && immersiveView.visible
                }

                // Integrated borderless seek bar + timestamps
                Item {
                    id: immSeekBlock
                    anchors.top: immSpectrum.visible ? immSpectrum.bottom : immBadgesRow.bottom
                    anchors.topMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: immLeftCol.ctrlW
                    height: 34

                    readonly property real ratio: root.controller.durationMs > 0
                        ? Math.max(0.0, Math.min(1.0, (immSeekMa.pressed ? immSeekMa.hoverRatio : (root.controller.positionMs / root.controller.durationMs))))
                        : 0.0

                    Item {
                        id: immSeekTrack
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 18
                        z: 10

                        Rectangle {
                            id: immSeekGroove
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: (immSeekMa.containsMouse || immSeekMa.pressed) ? 6 : 4
                            radius: height / 2
                            color: DesignTokens.playbackRail
                            Behavior on height { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

                            Rectangle {
                                visible: root.controller.durationMs > 0
                                width: parent.width * immSeekMa.hoverRatio
                                height: parent.height
                                radius: parent.radius
                                color: DesignTokens.isDark ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.12)
                                opacity: immSeekMa.containsMouse ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 120 } }
                            }

                            Rectangle {
                                width: parent.width * immSeekBlock.ratio
                                height: parent.height
                                radius: parent.radius
                                color: DesignTokens.accent
                            }
                        }

                        Rectangle {
                            width: 13
                            height: 13
                            radius: 6.5
                            anchors.verticalCenter: parent.verticalCenter
                            x: Math.max(0, Math.min(parent.width - width, parent.width * immSeekBlock.ratio - width / 2))
                            color: "#ffffff"
                            border.width: 2
                            border.color: DesignTokens.accent
                            scale: immSeekMa.pressed ? 1.24 : (immSeekMa.containsMouse && root.controller.durationMs > 0 ? 1.0 : 0.0)
                            opacity: (immSeekMa.containsMouse || immSeekMa.pressed) && root.controller.durationMs > 0 ? 1.0 : 0.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.15 } }
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }

                        Rectangle {
                            id: immHoverTimePill
                            readonly property bool shown: (immSeekMa.containsMouse || immSeekMa.pressed) && root.controller.durationMs > 0
                            visible: opacity > 0.01
                            opacity: shown ? 1.0 : 0.0
                            scale: shown ? 1.0 : 0.88
                            z: 40
                            width: immHoverTimeLabel.implicitWidth + 14
                            height: 20
                            radius: 10
                            y: -21
                            x: Math.max(0, Math.min(immSeekTrack.width - width, immSeekTrack.width * immSeekMa.hoverRatio - width / 2))
                            color: DesignTokens.surfaceElevated
                            border.width: 1
                            border.color: DesignTokens.borderHighlight
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }

                            Text {
                                id: immHoverTimeLabel
                                anchors.centerIn: parent
                                text: root.controller.formatTime(immSeekMa.hoverRatio * root.controller.durationMs)
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.features: ({ "tnum": 1 })
                            }
                        }

                        MouseArea {
                            id: immSeekMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: root.controller.durationMs > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                            property real hoverRatio: 0.0

                            function updateRatio(mx) {
                                hoverRatio = Math.max(0.0, Math.min(1.0, mx / Math.max(1, width)))
                            }
                            onPositionChanged: (mouse) => updateRatio(mouse.x)
                            onPressed: (mouse) => updateRatio(mouse.x)
                            onReleased: (mouse) => {
                                if (root.controller.durationMs > 0) {
                                    updateRatio(mouse.x)
                                    root.controller.seekRatio(hoverRatio)
                                }
                            }
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        text: root.controller.formatTime(immSeekMa.pressed ? immSeekMa.hoverRatio * root.controller.durationMs : root.controller.positionMs)
                        color: immSeekMa.pressed ? DesignTokens.accent : DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.weight: immSeekMa.pressed ? Font.DemiBold : Font.Normal
                        font.features: ({ "tnum": 1 })
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        text: root.controller.formatTime(root.controller.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.features: ({ "tnum": 1 })
                    }
                }

                // Integrated centered transport controls
                Row {
                    id: immTransportRow
                    anchors.top: immSeekBlock.bottom
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 42
                    spacing: 16

                    ImmIconBtn {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "shuffle"
                        iconSize: 15
                        active: root.controller.shuffle
                        showActiveDot: true
                        onClicked: root.controller.shuffle = !root.controller.shuffle
                    }

                    ImmIconBtn {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "prev"
                        iconSize: 18
                        onClicked: root.controller.previous()
                    }

                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 42
                        height: 42
                        scale: immPlayMa.pressed ? 0.90 : (immPlayMa.containsMouse ? 1.07 : 1.0)
                        Behavior on scale {
                            NumberAnimation { duration: 160; easing.type: Easing.OutBack; easing.overshoot: 1.15 }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: immPlayMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent

                            IconGlyph {
                                anchors.centerIn: parent
                                anchors.horizontalCenterOffset: root.controller.isPlaying ? 0 : 1
                                name: root.controller.isPlaying ? "pause" : "play"
                                size: 20
                                color: DesignTokens.playbackInk
                            }
                        }

                        MouseArea {
                            id: immPlayMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.controller.togglePlayPause()
                        }
                    }

                    ImmIconBtn {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "next"
                        iconSize: 18
                        onClicked: root.controller.next()
                    }

                    ImmIconBtn {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: root.controller.repeatMode === 2 ? "repeat_one" : "repeat"
                        iconSize: 15
                        active: root.controller.repeatMode !== 0
                        showActiveDot: true
                        onClicked: root.controller.cycleRepeatMode()
                    }
                }

                // Integrated utility bar (EQ + Track Info + Volume)
                RowLayout {
                    id: immUtilityRow
                    anchors.top: immTransportRow.bottom
                    anchors.topMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: immLeftCol.ctrlW
                    height: 30
                    spacing: 6

                    ImmIconBtn {
                        iconName: "eq"
                        iconSize: 15
                        active: root.eqOpen
                        onClicked: root.toggleEq()
                    }

                    ImmIconBtn {
                        iconName: "info"
                        iconSize: 15
                        visible: root.controller.hasTrack
                        onClicked: root.openTrackInfo()
                    }

                    Item { Layout.fillWidth: true }

                    ImmIconBtn {
                        iconName: (root.controller.muted || root.controller.volume <= 0.001) ? "volume_mute" : "volume"
                        iconSize: 15
                        onClicked: root.controller.muted = !root.controller.muted
                    }

                    VirelaSlider {
                        Layout.preferredWidth: 108
                        from: 0.0
                        to: 1.0
                        value: root.controller.muted ? 0.0 : root.controller.volume
                        accentColor: DesignTokens.accent
                        onMoved: (v) => {
                            root.controller.muted = false
                            root.controller.volume = v
                        }
                    }
                }
            }

            // Right Column: Completely Borderless Fluid Lyrics or Queue
            Item {
                id: immRightCol
                anchors.left: immLeftCol.right
                anchors.leftMargin: 20
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                // Borderless Synced Lyrics Viewport with Distance Depth Attenuation
                ListView {
                    id: immLyricsList
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    clip: true
                    spacing: 10
                    topMargin: Math.round(height * 0.36)
                    bottomMargin: Math.round(height * 0.50)
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.lyricsCtrl
                    preferredHighlightBegin: Math.round(height * 0.36)
                    preferredHighlightEnd: Math.round(height * 0.45)
                    highlightRangeMode: userScrolling ? ListView.NoHighlightRange : ListView.StrictlyEnforceRange
                    highlightMoveDuration: 340
                    currentIndex: root.lyricsCtrl.activeLineIndex >= 0 ? root.lyricsCtrl.activeLineIndex : 0
                    visible: root.activeTab === "lyrics" && root.lyricsCtrl.hasLyrics
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    property bool userScrolling: false

                    function recenter(immediate) {
                        if (!root.lyricsCtrl.hasLyrics || count <= 0) return
                        const idx = root.lyricsCtrl.activeLineIndex >= 0 ? root.lyricsCtrl.activeLineIndex : 0
                        if (immediate) positionViewAtIndex(idx, ListView.Center)
                    }

                    Component.onCompleted: Qt.callLater(function() { recenter(true) })
                    onHeightChanged: if (!userScrolling) Qt.callLater(function() { recenter(true) })
                    onCountChanged: Qt.callLater(function() { recenter(true) })
                    onMovementStarted: {
                        userScrolling = true
                        immResumeTimer.restart()
                    }

                    Timer {
                        id: immResumeTimer
                        interval: 3200
                        onTriggered: immLyricsList.userScrolling = false
                    }

                    delegate: Item {
                        id: ilRow
                        required property int index
                        required property int timestampMs
                        required property string text
                        required property string translation
                        required property bool isActive
                        required property bool hasWords
                        required property var wordTokens

                        width: immLyricsList.width - 14
                        height: ilCol.implicitHeight + 16

                        readonly property bool karaokeActive: ilRow.isActive && ilRow.hasWords && root.lyricsCtrl.isSynced
                        readonly property int dist: Math.abs(index - immLyricsList.currentIndex)
                        readonly property real viewRelY: y - immLyricsList.contentY
                        readonly property real topEdgeFade: Math.max(0.15, Math.min(1.0, (viewRelY + height * 0.5) / 64.0))
                        readonly property real botEdgeFade: Math.max(0.15, Math.min(1.0, (immLyricsList.height - viewRelY - height * 0.2) / 72.0))
                        readonly property real depthOpacity: ilRow.isActive ? 1.0
                            : (ilMa.containsMouse ? 0.94 : (dist === 1 ? 0.58 : (dist === 2 ? 0.36 : 0.22)) * Math.min(topEdgeFade, botEdgeFade))

                        Rectangle {
                            anchors.fill: parent
                            radius: 14
                            color: (ilMa.containsMouse && root.lyricsCtrl.isSynced)
                                ? DesignTokens.surfaceHover : "transparent"
                            Behavior on color { ColorAnimation { duration: 130 } }

                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                visible: ilMa.containsMouse && root.lyricsCtrl.isSynced && ilRow.timestampMs >= 0
                                text: root.controller.formatTime(ilRow.timestampMs)
                                color: DesignTokens.accent
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.features: ({ "tnum": 1 })
                            }
                        }

                        Column {
                            id: ilCol
                            anchors.left: parent.left
                            anchors.leftMargin: ilRow.isActive ? 16 : 12
                            anchors.right: parent.right
                            anchors.rightMargin: (ilMa.containsMouse && root.lyricsCtrl.isSynced) ? 62 : 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 5
                            transformOrigin: Item.Left
                            scale: ilRow.isActive ? 1.0 : 0.95

                            Behavior on anchors.leftMargin {
                                SpringAnimation { spring: 3.8; damping: 0.44; epsilon: 0.2 }
                            }
                            Behavior on scale {
                                SpringAnimation { spring: 3.8; damping: 0.42; epsilon: 0.002 }
                            }

                            // Word-by-word Enhanced LRC Karaoke Sweep (instantiated only on active line)
                            Flow {
                                visible: ilRow.karaokeActive
                                width: parent.width
                                spacing: 0

                                Repeater {
                                    model: ilRow.karaokeActive ? ilRow.wordTokens : []
                                    delegate: Item {
                                        id: wordSlot
                                        required property var modelData
                                        width: wordBase.implicitWidth
                                        height: wordBase.implicitHeight

                                        readonly property real spanMs: Math.max(1, modelData.endMs - modelData.startMs)
                                        readonly property real fillRatio: Math.max(0.0, Math.min(1.0,
                                            (root.liveEffectivePositionMs - modelData.startMs) / spanMs))
                                        readonly property real liftOffset: (fillRatio > 0.01 && fillRatio < 0.99)
                                            ? (-2.2 * Math.sin(fillRatio * Math.PI)) : 0.0

                                        Text {
                                            id: wordBase
                                            y: wordSlot.liftOffset
                                            text: wordSlot.modelData.text
                                            textFormat: Text.PlainText
                                            color: DesignTokens.textSecondary
                                            opacity: 0.40
                                            font.family: DesignTokens.fontFamily
                                            font.pixelSize: root.width < 990 ? 25 : 30
                                            font.weight: Font.Bold
                                        }

                                        Item {
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            height: wordBase.height
                                            width: wordBase.width * wordSlot.fillRatio
                                            clip: true

                                            Text {
                                                y: wordSlot.liftOffset
                                                text: wordBase.text
                                                textFormat: Text.PlainText
                                                color: DesignTokens.textPrimary
                                                font: wordBase.font
                                            }
                                        }
                                    }
                                }
                            }

                            // Pure Whole-Line Focus Highlight for ordinary line-level LRC
                            Text {
                                id: ilBaseText
                                visible: !ilRow.karaokeActive
                                width: parent.width
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                text: ilRow.text.length > 0 ? ilRow.text : "· · ·"
                                color: ilRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                opacity: ilRow.isActive ? 1.0 : ilRow.depthOpacity
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: ilRow.isActive ? (root.width < 990 ? 25 : 30) : 22
                                font.weight: ilRow.isActive ? Font.Bold : Font.Medium
                                Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation { duration: 220 } }
                            }

                            Text {
                                width: parent.width
                                visible: ilRow.translation.length > 0
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                text: ilRow.translation
                                color: ilRow.isActive ? DesignTokens.textSecondary : DesignTokens.textTertiary
                                opacity: ilRow.isActive ? 0.94 : ilRow.depthOpacity * 0.78
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                Behavior on opacity { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
                            }
                        }

                        MouseArea {
                            id: ilMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: root.lyricsCtrl.isSynced ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.lyricsCtrl.isSynced) {
                                    const ts = root.lyricsCtrl.timestampForLine(ilRow.index)
                                    if (ts >= 0) {
                                        immLyricsList.userScrolling = false
                                        root.controller.seekMs(ts)
                                    }
                                }
                            }
                        }
                    }
                }

                // Immersive Empty Lyrics State
                GlassPanel {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 370)
                    height: immNoLyrCol.implicitHeight + 44
                    radius: DesignTokens.radiusXl
                    shadowEnabled: false
                    visible: root.activeTab === "lyrics" && !root.lyricsCtrl.hasLyrics

                    ColumnLayout {
                        id: immNoLyrCol
                        anchors.centerIn: parent
                        width: parent.width - 44
                        spacing: 10

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 52; implicitHeight: 52; radius: 16
                            color: DesignTokens.accentSoft
                            IconGlyph { anchors.centerIn: parent; name: "lyrics"; size: 24; color: DesignTokens.accent }
                        }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("No Lyrics Available")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: qsTr("Place a matching .lrc file next to the audio file or embed USLT/LYRICS tags.")
                            color: DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                        }
                    }
                }

                // Immersive Borderless Queue View
                ListView {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    visible: root.activeTab === "queue"
                    clip: true
                    spacing: 5
                    reuseItems: true
                    cacheBuffer: 240
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.controller.queueModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: iqRow
                        required property int index
                        required property string title
                        required property string artist
                        required property string coverHash
                        required property int durationMs
                        required property bool isCurrent

                        readonly property bool hovered: iqMa.containsMouse || iqRmMa.containsMouse
                        readonly property bool nextStartsHere: root.controller.currentIndex >= 0
                            && index === root.controller.currentIndex + 1
                        width: ListView.view ? ListView.view.width : 400
                        height: 48 + (iqGroupLbl.visible ? 22 : 0)

                        Text {
                            id: iqGroupLbl
                            visible: iqRow.isCurrent || iqRow.nextStartsHere
                            height: visible ? 22 : 0
                            verticalAlignment: Text.AlignVCenter
                            text: iqRow.isCurrent ? qsTr("Now Playing") : qsTr("Up Next")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            anchors.top: iqGroupLbl.bottom
                            anchors.bottom: parent.bottom
                            width: parent.width
                            radius: DesignTokens.radiusMd
                            color: iqRow.isCurrent ? DesignTokens.surfaceSelected
                                : (iqRow.hovered ? DesignTokens.surfaceHover : "transparent")
                            Behavior on color { ColorAnimation { duration: 110 } }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.topMargin: iqGroupLbl.height
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                radius: DesignTokens.radiusSm
                                color: DesignTokens.surfaceCard
                                clip: true

                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: iqCover.status !== Image.Ready
                                    name: "music"
                                    size: 16
                                    color: DesignTokens.textTertiary
                                }
                                Image {
                                    id: iqCover
                                    anchors.fill: parent
                                    source: iqRow.isCurrent ? root.controller.trackCoverUrl
                                        : (iqRow.coverHash.length > 0 ? "image://cover/" + iqRow.coverHash + "?size=96" : "")
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: iqRow.title
                                    elide: Text.ElideRight
                                    color: iqRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 13
                                    font.weight: iqRow.isCurrent ? Font.DemiBold : Font.Medium
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: iqRow.artist.length > 0 ? iqRow.artist : qsTr("Unknown Artist")
                                    elide: Text.ElideRight
                                    color: DesignTokens.textTertiary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 11
                                }
                            }

                            NowPlayingBars {
                                visible: iqRow.isCurrent && !iqRow.hovered
                                playing: root.controller.isPlaying
                                size: 14
                            }

                            Text {
                                visible: !iqRow.isCurrent && !iqRow.hovered
                                text: root.controller.formatTime(iqRow.durationMs)
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                                font.features: ({ "tnum": 1 })
                            }

                            Rectangle {
                                visible: iqRow.hovered
                                implicitWidth: 26
                                implicitHeight: 26
                                radius: 7
                                color: iqRmMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "close"
                                    size: 11
                                    color: iqRmMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                }
                                MouseArea {
                                    id: iqRmMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.controller.removeFromQueue(iqRow.index)
                                }
                            }
                        }

                        MouseArea {
                            id: iqMa
                            anchors.fill: parent
                            anchors.topMargin: iqGroupLbl.height
                            anchors.rightMargin: 34
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.controller.playQueueIndex(iqRow.index)
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        visible: root.controller.queueCount === 0
                        spacing: 8
                        IconGlyph {
                            Layout.alignment: Qt.AlignHCenter
                            name: "queue"
                            size: 28
                            color: DesignTokens.textTertiary
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Queue is empty\nDrop audio files anywhere to start listening")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            lineHeight: 1.45
                        }
                    }
                }
            }
        }
    }
}
