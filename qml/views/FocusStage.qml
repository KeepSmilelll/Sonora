import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Sonora.UI

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
    property int trackSwitchDirection: 1 // 1 = forward, -1 = backward

    readonly property real liveBassEnergy: (root.controller.isPlaying
        && root.settingsObj && root.settingsObj.visualizerEnabled
        && root.stageMode !== "collapsed")
        ? (root.immersiveFactor >= 0.5 ? immSpectrum.bassEnergy : railSpectrum.bassEnergy)
        : 0.0

    signal requestImmersiveToggle()
    signal requestCollapse()
    signal toggleFavorite()
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

    // Smooth 60fps interpolated active-line progress for pixel-smooth text sweep
    property real smoothLineProgress: 0.0
    property bool _allowProgressSmooth: true
    property int _lastProgressLineIdx: -1

    Behavior on smoothLineProgress {
        enabled: root._allowProgressSmooth
        NumberAnimation { duration: 48; easing.type: Easing.Linear }
    }

    Connections {
        target: root.lyricsCtrl
        function onPositionProgressChanged() {
            const idx = root.lyricsCtrl.activeLineIndex
            const nextP = root.lyricsCtrl.activeLineProgress
            if (idx !== root._lastProgressLineIdx || nextP < root.smoothLineProgress - 0.12) {
                root._allowProgressSmooth = false
                root._lastProgressLineIdx = idx
                root.smoothLineProgress = nextP
                root._allowProgressSmooth = true
            } else {
                root.smoothLineProgress = nextP
            }
        }
        function onLyricsChanged() {
            root._allowProgressSmooth = false
            root._lastProgressLineIdx = -1
            root.smoothLineProgress = 0.0
            root._allowProgressSmooth = true
        }
    }

    function escapeHtml(str) {
        return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function styledProgressText(str, progress, activeCol, mutedCol) {
        if (!str || str.length === 0) return ""
        const p = Math.max(0.0, Math.min(1.0, progress))
        const splitIdx = Math.round(str.length * p)
        const head = escapeHtml(str.slice(0, splitIdx))
        const tail = escapeHtml(str.slice(splitIdx))
        return "<font color=\"" + activeCol + "\">" + head + "</font>"
             + "<font color=\"" + mutedCol + "\">" + tail + "</font>"
    }

    function splitLineRuns(str) {
        if (!str || str.length === 0) return []
        const raw = []
        let i = 0
        while (i < str.length) {
            const ch = str.charCodeAt(i)
            if (ch <= 0x024F) {
                let j = i
                while (j < str.length && str.charCodeAt(j) <= 0x024F && str[j] !== " ") j++
                while (j < str.length && str[j] === " ") j++
                raw.push(str.slice(i, j))
                i = j
            } else {
                let j = i + 1
                while (j < str.length && str[j] === " ") j++
                raw.push(str.slice(i, j))
                i = j
            }
        }
        let total = 0
        for (let k = 0; k < raw.length; k++) total += Math.max(1, raw[k].length)
        const runs = []
        let acc = 0
        for (let k = 0; k < raw.length; k++) {
            const w = Math.max(1, raw[k].length)
            runs.push({
                text: raw[k],
                startFrac: acc / total,
                endFrac: (acc + w) / total
            })
            acc += w
        }
        return runs
    }

    // Deterministic closed-form circular vinyl geometry shared by QML items & HeroTransitionLayer
    function computeRailCoverGeometry(stageW) {
        const art = Math.round(Math.max(124, Math.min(196, stageW - 76)))
        return {
            x: Math.round((stageW - art) * 0.5),
            y: 68,
            width: art,
            height: art,
            radius: art * 0.5
        }
    }

    function computeImmersiveCoverGeometry(stageW, stageH, vizEnabled) {
        const bodyW = Math.max(300, stageW - 32)
        const bodyH = Math.max(240, stageH - 76)
        const leftColW = Math.round(Math.max(270, Math.min(420, bodyW * 0.37)))
        const art = Math.round(Math.max(176, Math.min(324, leftColW - 44, bodyH - 210)))
        const groupH = art + 14 + 54 + 14 + 28 + (vizEnabled ? 52 : 0)
        const groupY = Math.max(8, Math.round((bodyH - groupH) * 0.46))
        return {
            leftColW: leftColW,
            groupH: groupH,
            groupY: groupY,
            x: 16 + Math.round((leftColW - art) * 0.5),
            y: 60 + groupY,
            width: art,
            height: art,
            radius: art * 0.5
        }
    }

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
        anchors.topMargin: 14
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

        // Lyric offset & source badge (visible in Immersive Lyrics mode)
        RowLayout {
            visible: root.immersiveFactor > 0.6 && root.activeTab === "lyrics" && root.lyricsCtrl.isSynced
            opacity: root.immersiveFactor
            spacing: 6

            SonoraButton {
                compact: true
                text: "-0.25s"
                onClicked: root.lyricsCtrl.adjustOffsetMs(-250)
            }
            Text {
                text: (root.lyricsCtrl.userOffsetMs >= 0 ? "+" : "") + (root.lyricsCtrl.userOffsetMs / 1000.0).toFixed(2) + "s"
                color: DesignTokens.textSecondary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
                font.features: ({ "tnum": 1 })
            }
            SonoraButton {
                compact: true
                text: "+0.25s"
                onClicked: root.lyricsCtrl.adjustOffsetMs(250)
            }
        }

        FormatBadge {
            visible: root.immersiveFactor > 0.6 && root.activeTab === "lyrics" && root.lyricsCtrl.sourceLabel.length > 0
            text: root.lyricsCtrl.sourceLabel
        }

        SonoraButton {
            visible: root.activeTab === "queue" && root.controller.queueCount > 0
            compact: true
            text: qsTr("Clear")
            onClicked: root.controller.clearQueue()
        }

        // Expand / Collapse Immersive Toggle
        Rectangle {
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

                            // Text-rendered progress sweep (no underline bars)
                            Item {
                                id: rlTextHost
                                width: parent.width
                                height: rlBaseText.implicitHeight

                                readonly property bool karaokeFill: rlRow.isActive && root.lyricsCtrl.isSynced
                                readonly property bool multiLineFill: karaokeFill && rlBaseText.lineCount > 1

                                Text {
                                    id: rlBaseText
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    textFormat: rlTextHost.multiLineFill ? Text.StyledText : Text.PlainText
                                    text: {
                                        const raw = rlRow.text.length > 0 ? rlRow.text : "· · ·"
                                        if (rlTextHost.multiLineFill) {
                                            return root.styledProgressText(
                                                raw, root.smoothLineProgress,
                                                DesignTokens.textPrimary.toString(),
                                                Qt.rgba(DesignTokens.textSecondary.r, DesignTokens.textSecondary.g, DesignTokens.textSecondary.b, 0.44).toString())
                                        }
                                        return raw
                                    }
                                    color: rlTextHost.karaokeFill
                                        ? DesignTokens.textSecondary
                                        : (rlRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                                    opacity: rlTextHost.karaokeFill && !rlTextHost.multiLineFill
                                        ? 0.40 : (rlTextHost.multiLineFill ? 1.0 : rlRow.depthOpacity)
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: rlRow.isActive ? 15 : 13
                                    font.weight: rlRow.isActive ? Font.Bold : Font.Medium
                                    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }

                                // Pixel-accurate left-to-right sweep overlay for single-line active lyrics
                                Item {
                                    visible: rlTextHost.karaokeFill && !rlTextHost.multiLineFill
                                    x: Math.round((rlTextHost.width - rlBaseText.paintedWidth) * 0.5)
                                    y: 0
                                    height: rlBaseText.height
                                    width: rlBaseText.paintedWidth * root.smoothLineProgress
                                    clip: true

                                    Text {
                                        text: rlRow.text.length > 0 ? rlRow.text : "· · ·"
                                        textFormat: Text.PlainText
                                        color: DesignTokens.textPrimary
                                        font: rlBaseText.font
                                    }
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

            // Left Column: Hero Artwork + Typography + Wide Spectrum
            Item {
                id: immLeftCol
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: immersiveView.immGeom.leftColW

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
                    height: 54
                    spacing: 4

                    Text {
                        width: parent.width
                        height: 30
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
                    anchors.topMargin: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 28
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
                    anchors.topMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.round(immersiveCoverBox.width * 0.92)
                    height: 44
                    styleMode: "aurora"
                    visible: root.settingsObj.visualizerEnabled && root.immersiveFactor > 0.25
                    accentColor: DesignTokens.accent
                    secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                    playing: root.controller.isPlaying
                    visualizerEnabled: root.settingsObj.visualizerEnabled && immersiveView.visible
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

                        readonly property bool karaokeActive: ilRow.isActive && ilRow.hasWords
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
                                            (root.lyricsCtrl.effectivePositionMs - modelData.startMs) / spanMs))
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

                            // Standard LRC Active Line (pixel-smooth text sweep, zero underline bars)
                            Item {
                                id: ilTextHost
                                visible: !ilRow.karaokeActive
                                width: parent.width
                                height: ilBaseText.implicitHeight

                                readonly property bool lineKaraoke: ilRow.isActive && !ilRow.hasWords && root.lyricsCtrl.isSynced
                                readonly property bool multiLineRuns: lineKaraoke && ilBaseText.lineCount > 1

                                Text {
                                    id: ilBaseText
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    text: ilRow.text.length > 0 ? ilRow.text : "· · ·"
                                    color: ilTextHost.lineKaraoke
                                        ? DesignTokens.textSecondary
                                        : (ilRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                                    opacity: ilTextHost.multiLineRuns
                                        ? 0.0
                                        : (ilTextHost.lineKaraoke ? 0.40 : ilRow.depthOpacity)
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: ilRow.isActive ? (root.width < 990 ? 25 : 30) : 22
                                    font.weight: ilRow.isActive ? Font.Bold : Font.Medium
                                    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 220 } }
                                }

                                // Single-line active lyric pixel sweep
                                Item {
                                    visible: ilTextHost.lineKaraoke && !ilTextHost.multiLineRuns
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    height: ilBaseText.height
                                    width: ilBaseText.paintedWidth * root.smoothLineProgress
                                    clip: true

                                    Text {
                                        text: ilBaseText.text
                                        textFormat: Text.PlainText
                                        color: DesignTokens.textPrimary
                                        font: ilBaseText.font
                                    }
                                }

                                // Multi-line wrapped active lyric word/char flow sweep
                                Flow {
                                    visible: ilTextHost.multiLineRuns
                                    width: parent.width
                                    spacing: 0

                                    Repeater {
                                        model: ilTextHost.multiLineRuns ? root.splitLineRuns(ilRow.text) : []
                                        delegate: Item {
                                            id: runSlot
                                            required property var modelData
                                            width: runBase.implicitWidth
                                            height: runBase.implicitHeight

                                            readonly property real span: Math.max(1e-4, modelData.endFrac - modelData.startFrac)
                                            readonly property real fillRatio: Math.max(0.0, Math.min(1.0,
                                                (root.smoothLineProgress - modelData.startFrac) / span))

                                            Text {
                                                id: runBase
                                                text: runSlot.modelData.text
                                                textFormat: Text.PlainText
                                                color: DesignTokens.textSecondary
                                                opacity: 0.40
                                                font: ilBaseText.font
                                            }

                                            Item {
                                                anchors.left: parent.left
                                                anchors.top: parent.top
                                                height: runBase.height
                                                width: runBase.width * runSlot.fillRatio
                                                clip: true

                                                Text {
                                                    text: runBase.text
                                                    textFormat: Text.PlainText
                                                    color: DesignTokens.textPrimary
                                                    font: ilBaseText.font
                                                }
                                            }
                                        }
                                    }
                                }
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
