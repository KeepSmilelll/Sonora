import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Virela.UI

// Hover-expandable morphing desktop capsule with live single-line lyrics & mini aurora spectrum.
// Fixed transparent HWND bounds (396x118) eliminate Win32 window resize flicker while inner capsule morphs at 60fps.
Window {
    id: miniWin

    required property var controller
    property var lyricsCtrl: null
    property var settingsObj: null
    signal restoreMainRequested()

    width: 396
    height: 118
    minimumWidth: 396
    maximumWidth: 396
    minimumHeight: 118
    maximumHeight: 118
    visible: true
    transientParent: null
    title: "Virela Mini"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    onClosing: (close) => {
        if (!platformBridge.quitting) {
            close.accepted = false
            miniWin.restoreMainRequested()
        }
    }

    component MiniIconBtn: Rectangle {
        id: btn
        property string iconName: "prev"
        property real iconSize: 14
        signal clicked()

        implicitWidth: 28
        implicitHeight: 28
        radius: 14
        color: btnMa.pressed
            ? DesignTokens.surfacePressed
            : (btnMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
        scale: btnMa.pressed ? 0.88 : (btnMa.containsMouse ? 1.08 : 1.0)
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }

        IconGlyph {
            anchors.centerIn: parent
            name: btn.iconName
            size: btn.iconSize
            color: btnMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
        }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    GlassPanel {
        id: capsule
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: expanded ? 114 : 64
        radius: expanded ? 22 : 32
        panelColor: DesignTokens.dockFloatingBg
        borderColor: DesignTokens.borderHighlight
        specularEnabled: true
        shadowEnabled: false

        readonly property bool expanded: hoverTracker.hovered || seekArea.pressed || expandGraceTimer.running

        Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutQuint } }
        Behavior on radius { NumberAnimation { duration: 250; easing.type: Easing.OutQuint } }

        HoverHandler {
            id: hoverTracker
            onHoveredChanged: {
                if (!hovered) {
                    expandGraceTimer.restart()
                } else {
                    expandGraceTimer.stop()
                }
            }
        }

        Timer {
            id: expandGraceTimer
            interval: 360
        }

        // Subtle cover-matched aurora tint inside the floating capsule
        FluidAuroraItem {
            anchors.fill: parent
            primaryColor: DesignTokens.ambientPrimary
            secondaryColor: DesignTokens.ambientSecondary
            accentColor: DesignTokens.accent
            baseColor: "transparent"
            darkMode: DesignTokens.isDark
            active: miniWin.visible && DesignTokens.backdropMode !== "solid"
            playing: miniWin.controller.isPlaying
            intensity: 0.55
            opacity: DesignTokens.backdropMode === "solid" ? 0.0 : 0.24
        }

        // Drag area across top compact bar
        Item {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64

            DragHandler {
                target: null
                onActiveChanged: if (active) miniWin.startSystemMove()
            }
        }

        // Top Compact Row (always visible)
        RowLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            // Morphing Cover (circle when compact -> rounded square when expanded)
            Rectangle {
                id: miniCoverBox
                implicitWidth: 44
                implicitHeight: 44
                radius: capsule.expanded ? 12 : 22
                color: DesignTokens.surfaceCard
                border.width: 1
                border.color: miniWin.controller.isPlaying ? DesignTokens.accent : DesignTokens.borderHighlight
                clip: true

                Behavior on radius { NumberAnimation { duration: 240; easing.type: Easing.OutQuint } }
                Behavior on border.color { ColorAnimation { duration: 180 } }

                Rectangle {
                    id: miniCoverMask
                    anchors.fill: parent
                    radius: miniCoverBox.radius
                    visible: false
                    layer.enabled: true
                    layer.smooth: true
                }

                Image {
                    id: miniCoverImg
                    anchors.fill: parent
                    source: miniWin.controller.trackCoverUrl
                    sourceSize: Qt.size(96, 96)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: false
                }

                MultiEffect {
                    anchors.fill: miniCoverImg
                    visible: miniCoverImg.status === Image.Ready
                    source: miniCoverImg
                    maskEnabled: true
                    maskSource: miniCoverMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "music"
                    size: 20
                    color: DesignTokens.textTertiary
                    visible: miniCoverImg.status !== Image.Ready
                }
            }

            // Title + Live Single-Line Lyric (falls back cleanly to Artist)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                readonly property bool showingLyric: Boolean(
                    miniWin.lyricsCtrl && miniWin.lyricsCtrl.hasLyrics
                    && miniWin.lyricsCtrl.activeLineText.length > 0)

                Text {
                    Layout.fillWidth: true
                    text: miniWin.controller.hasTrack ? miniWin.controller.trackTitle : "Virela"
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: parent.showingLyric
                        ? miniWin.lyricsCtrl.activeLineText
                        : (miniWin.controller.trackArtist.length > 0 ? miniWin.controller.trackArtist : qsTr("Ready to play"))
                    elide: Text.ElideRight
                    color: parent.showingLyric ? DesignTokens.accentLight : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: parent.showingLyric ? Font.Medium : Font.Normal
                    Behavior on color { ColorAnimation { duration: 180 } }
                }
            }

            // Compact Aurora Spectrum (visible in collapsed capsule while playing)
            SpectrumVisualizerItem {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 18
                styleMode: "aurora"
                visible: !capsule.expanded
                    && Boolean(miniWin.settingsObj && miniWin.settingsObj.visualizerEnabled)
                    && miniWin.controller.isPlaying
                accentColor: DesignTokens.accent
                secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                playing: miniWin.controller.isPlaying
                visualizerEnabled: visible
            }

            // Play / Pause primary circle
            Item {
                implicitWidth: 32
                implicitHeight: 32
                scale: miniPlayMa.pressed ? 0.90 : (miniPlayMa.containsMouse ? 1.08 : 1.0)
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    color: miniPlayMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent

                    IconGlyph {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: miniWin.controller.isPlaying ? 0 : 1
                        name: miniWin.controller.isPlaying ? "pause" : "play"
                        size: 16
                        color: DesignTokens.playbackInk
                    }
                }

                MouseArea {
                    id: miniPlayMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: miniWin.controller.togglePlayPause()
                }
            }

            MiniIconBtn {
                iconName: "maximize"
                iconSize: 12
                onClicked: miniWin.restoreMainRequested()
            }
        }

        // Bottom Expanded Transport & Seek Row (slides open on hover)
        Item {
            id: expandedDrawer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 8
            height: 40
            opacity: capsule.expanded ? 1.0 : 0.0
            visible: opacity > 0.01

            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: DesignTokens.hairlineBorder
            }

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: 4
                spacing: 8

                MiniIconBtn {
                    iconName: "prev"
                    iconSize: 14
                    onClicked: miniWin.controller.previous()
                }

                Text {
                    text: miniWin.controller.formatTime(seekArea.pressed
                        ? seekArea.dragRatio * miniWin.controller.durationMs
                        : miniWin.controller.positionMs)
                    color: seekArea.pressed ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 10
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 32
                    horizontalAlignment: Text.AlignRight
                }

                MouseArea {
                    id: seekArea
                    Layout.fillWidth: true
                    implicitHeight: 22
                    enabled: miniWin.controller.durationMs > 0
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.PointingHandCursor

                    property real dragRatio: 0.0
                    readonly property bool interacting: enabled && (containsMouse || pressed)
                    readonly property real displayedRatio: !enabled ? 0.0 : (pressed ? dragRatio
                        : Math.max(0.0, Math.min(1.0, miniWin.controller.positionMs / miniWin.controller.durationMs)))

                    function ratioAt(x) {
                        return Math.max(0.0, Math.min(1.0, x / Math.max(1, width)))
                    }

                    onPressed: (mouse) => dragRatio = ratioAt(mouse.x)
                    onPositionChanged: (mouse) => {
                        if (pressed) dragRatio = ratioAt(mouse.x)
                    }
                    onReleased: (mouse) => miniWin.controller.seekRatio(ratioAt(mouse.x))

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: seekArea.interacting ? 5 : 3
                        radius: height / 2
                        color: DesignTokens.playbackRail
                        Behavior on height { NumberAnimation { duration: 120 } }

                        Rectangle {
                            height: parent.height
                            width: parent.width * seekArea.displayedRatio
                            radius: height / 2
                            color: DesignTokens.accent
                        }
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Math.max(0, Math.min(seekArea.width - width,
                            seekArea.width * seekArea.displayedRatio - width / 2))
                        width: 10
                        height: 10
                        radius: 5
                        color: "#ffffff"
                        border.width: 2
                        border.color: DesignTokens.accent
                        opacity: seekArea.interacting ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                }

                Text {
                    text: miniWin.controller.formatTime(miniWin.controller.durationMs)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 10
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 32
                }

                MiniIconBtn {
                    iconName: "next"
                    iconSize: 14
                    onClicked: miniWin.controller.next()
                }
            }
        }
    }
}
