import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Sonora.UI

GlassPanel {
    id: root

    required property var controller
    property bool lyricsOpen: false
    property bool compact: false
    property bool eqOpen: false
    property bool queueOpen: false
    property string stageMode: "rail"
    property string stageTab: "lyrics"
    property real immersiveFactor: 0.0
    property bool hideCoverForHero: false

    readonly property bool showTrackIdentity: stageMode === "collapsed" || !controller.hasTrack

    signal toggleLyrics()
    signal toggleEq()
    signal toggleQueue()
    signal openTrackInfo()
    signal toggleFavorite()
    signal coverClicked()

    function coverAnchorItem() {
        return coverButton
    }

    function currentVinylAngle() {
        return dockVinyl.discRotation
    }

    function predictedCoverRectInWindow(winWidth, winHeight, targetDockWidth, isCompact) {
        const dockX = (winWidth - targetDockWidth) * 0.5
        const dockY = winHeight - DesignTokens.dockMarginBottom - DesignTokens.dockHeight
        const marginL = isCompact ? 12 : 16
        return {
            x: Math.round(dockX + marginL),
            y: Math.round(dockY + (DesignTokens.dockHeight - 46) * 0.5),
            width: 46,
            height: 46,
            radius: 23
        }
    }

    implicitHeight: DesignTokens.dockHeight
    radius: 24
    panelColor: immersiveFactor > 0.5 ? DesignTokens.dockImmersiveBg : DesignTokens.dockFloatingBg
    borderColor: DesignTokens.borderOuter
    specularEnabled: true
    shadowEnabled: true

    component DockIconBtn: Item {
        id: btn
        property string iconName: "play"
        property bool active: false
        property bool showActiveDot: false
        property real iconSize: 16
        signal clicked()

        implicitWidth: 29
        implicitHeight: 29

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

        // Glowing active indicator dot under Shuffle / Repeat
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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 12 : 16
        anchors.rightMargin: root.compact ? 12 : 16
        spacing: root.compact ? 12 : 23

        // Left: Cover and track metadata (auto-collapses cleanly when FocusStage displays cover)
        RowLayout {
            id: trackIdentityBox
            Layout.fillWidth: false
            Layout.preferredWidth: root.showTrackIdentity ? (root.compact ? 190 : 254) : 0
            Layout.maximumWidth: Layout.preferredWidth
            opacity: root.showTrackIdentity ? 1.0 : 0.0
            visible: Layout.preferredWidth > 2
            clip: true
            spacing: 10

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 240; easing.type: Easing.OutQuint }
            }
            Behavior on opacity {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            Item {
                id: coverButton
                implicitWidth: 46
                implicitHeight: 46
                opacity: root.hideCoverForHero ? 0.0 : 1.0

                readonly property bool hovered: coverMa.containsMouse && root.controller.hasTrack
                readonly property bool indicatingAction: hovered || coverMa.pressed

                Item {
                    id: coverCard
                    anchors.fill: parent

                    scale: coverMa.pressed ? 0.96 : (coverButton.hovered ? 1.07 : 1.0)
                    Behavior on scale {
                        NumberAnimation { duration: DesignTokens.animFast; easing.type: Easing.OutCubic }
                    }

                    VinylDiscItem {
                        id: dockVinyl
                        anchors.fill: parent
                        compact: true
                        ownSpin: true
                        playing: root.controller.isPlaying
                        coverUrl: root.controller.trackCoverUrl
                        hovered: coverButton.indicatingAction
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: width * 0.5
                        color: Qt.rgba(0, 0, 0, coverMa.pressed ? 0.52 : 0.36)
                        opacity: coverButton.indicatingAction ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: DesignTokens.animFast } }

                        IconGlyph {
                            anchors.centerIn: parent
                            name: "expand"
                            size: 15
                            color: "#ffffff"
                        }
                    }
                }

                MouseArea {
                    id: coverMa
                    anchors.fill: parent
                    enabled: root.controller.hasTrack
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.coverClicked()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: root.controller.hasTrack ? root.controller.trackTitle : qsTr("No Track Playing")
                        elide: Text.ElideRight
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Item {
                        id: favBtn
                        visible: root.controller.hasTrack && root.controller.trackId >= 0
                        implicitWidth: 24
                        implicitHeight: 24
                        property real popScale: 1.0
                        scale: popScale

                        SequentialAnimation {
                            id: heartPopAnim
                            NumberAnimation { target: favBtn; property: "popScale"; to: 1.35; duration: 110; easing.type: Easing.OutQuad }
                            NumberAnimation { target: favBtn; property: "popScale"; to: 0.92; duration: 95; easing.type: Easing.InOutQuad }
                            NumberAnimation { target: favBtn; property: "popScale"; to: 1.0; duration: 130; easing.type: Easing.OutBack }
                        }

                        IconGlyph {
                            anchors.centerIn: parent
                            name: root.controller.trackIsFavorite ? "heart_filled" : "heart"
                            size: 14
                            color: root.controller.trackIsFavorite
                                ? DesignTokens.accent
                                : (favMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textTertiary)
                        }

                        MouseArea {
                            id: favMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                heartPopAnim.restart()
                                root.toggleFavorite()
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.controller.hasTrack
                        ? (root.controller.trackArtist.length > 0 ? root.controller.trackArtist : qsTr("Unknown Artist"))
                        : qsTr("Drop audio files or folders to play")
                    elide: Text.ElideRight
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }
        }

        // Center: Transport buttons + interactive seek bar with floating hover time pill
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            z: 10

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: root.compact ? 9 : 15

                DockIconBtn {
                    iconName: "shuffle"
                    iconSize: 15
                    active: root.controller.shuffle
                    showActiveDot: true
                    onClicked: root.controller.shuffle = !root.controller.shuffle
                }

                DockIconBtn {
                    iconName: "prev"
                    iconSize: 17
                    onClicked: root.controller.previous()
                }

                // Primary playback button.
                Item {
                    id: playPauseBtn
                    implicitWidth: 38
                    implicitHeight: 38

                    scale: playMa.pressed ? 0.90 : (playMa.containsMouse ? 1.07 : 1.0)
                    Behavior on scale {
                        NumberAnimation { duration: 160; easing.type: Easing.OutBack; easing.overshoot: 1.15 }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: playMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent

                        IconGlyph {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: root.controller.isPlaying ? 0 : 1
                            name: root.controller.isPlaying ? "pause" : "play"
                            size: 19
                            color: DesignTokens.playbackInk
                        }
                    }

                    MouseArea {
                        id: playMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.controller.togglePlayPause()
                    }
                }

                DockIconBtn {
                    iconName: "next"
                    iconSize: 17
                    onClicked: root.controller.next()
                }

                DockIconBtn {
                    iconName: root.controller.repeatMode === 2 ? "repeat_one" : "repeat"
                    iconSize: 15
                    active: root.controller.repeatMode !== 0
                    showActiveDot: true
                    onClicked: root.controller.cycleRepeatMode()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 550
                Layout.alignment: Qt.AlignHCenter
                spacing: 9

                Text {
                    text: root.controller.formatTime(seekMa.pressed ? seekMa.hoverRatio * root.controller.durationMs : root.controller.positionMs)
                    color: seekMa.pressed ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: seekMa.pressed ? Font.DemiBold : Font.Normal
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 30
                    horizontalAlignment: Text.AlignRight
                }

                Item {
                    id: seekTrack
                    Layout.fillWidth: true
                    implicitHeight: 20
                    z: 20

                    readonly property real ratio: root.controller.durationMs > 0
                        ? Math.max(0.0, Math.min(1.0, (seekMa.pressed ? seekMa.hoverRatio : (root.controller.positionMs / root.controller.durationMs))))
                        : 0.0

                    Rectangle {
                        id: seekGroove
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: (seekMa.containsMouse || seekMa.pressed) ? 6 : 4
                        radius: height / 2
                        color: DesignTokens.playbackRail
                        Behavior on height { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

                        // Ghost hover preview bar
                        Rectangle {
                            visible: root.controller.durationMs > 0
                            width: parent.width * seekMa.hoverRatio
                            height: parent.height
                            radius: parent.radius
                            color: DesignTokens.isDark ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.12)
                            opacity: seekMa.containsMouse ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }

                        Rectangle {
                            width: parent.width * seekTrack.ratio
                            height: parent.height
                            radius: parent.radius
                            color: DesignTokens.accent
                        }
                    }

                    // Tactile seek thumb
                    Rectangle {
                        width: 13
                        height: 13
                        radius: 6.5
                        anchors.verticalCenter: parent.verticalCenter
                        x: Math.max(0, Math.min(parent.width - width, parent.width * seekTrack.ratio - width / 2))
                        color: "#ffffff"
                        border.width: 2
                        border.color: DesignTokens.accent
                        scale: seekMa.pressed ? 1.24 : (seekMa.containsMouse && root.controller.durationMs > 0 ? 1.0 : 0.0)
                        opacity: (seekMa.containsMouse || seekMa.pressed) && root.controller.durationMs > 0 ? 1.0 : 0.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.15 } }
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }

                    // Floating hover time-preview capsule
                    Rectangle {
                        id: hoverTimePill
                        readonly property bool shown: (seekMa.containsMouse || seekMa.pressed) && root.controller.durationMs > 0
                        visible: opacity > 0.01
                        opacity: shown ? 1.0 : 0.0
                        scale: shown ? 1.0 : 0.88
                        z: 50
                        width: hoverTimeLabel.implicitWidth + 14
                        height: 20
                        radius: 10
                        y: -21
                        x: Math.max(0, Math.min(seekTrack.width - width, seekTrack.width * seekMa.hoverRatio - width / 2))
                        color: DesignTokens.surfaceElevated
                        border.width: 1
                        border.color: DesignTokens.borderHighlight

                        Behavior on opacity { NumberAnimation { duration: 120 } }
                        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }

                        Text {
                            id: hoverTimeLabel
                            anchors.centerIn: parent
                            text: root.controller.formatTime(seekMa.hoverRatio * root.controller.durationMs)
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    MouseArea {
                        id: seekMa
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
                    text: root.controller.formatTime(root.controller.durationMs)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 30
                }
            }
        }

        // Right: Lyrics, EQ, Info, Queue, Mute + SonoraSlider Volume
        RowLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: root.compact ? 214 : 252
            Layout.alignment: Qt.AlignRight
            spacing: root.compact ? 4 : 6

            Item { Layout.fillWidth: true }

            DockIconBtn {
                iconName: "lyrics"
                iconSize: 15
                active: root.lyricsOpen
                onClicked: root.toggleLyrics()
            }

            DockIconBtn {
                iconName: "eq"
                iconSize: 15
                active: root.eqOpen
                onClicked: root.toggleEq()
            }

            DockIconBtn {
                iconName: "info"
                iconSize: 15
                visible: root.controller.hasTrack
                onClicked: root.openTrackInfo()
            }

            DockIconBtn {
                iconName: "queue"
                iconSize: 15
                active: root.queueOpen
                onClicked: root.toggleQueue()
            }

            DockIconBtn {
                iconName: (root.controller.muted || root.controller.volume <= 0.001) ? "volume_mute" : "volume"
                iconSize: 15
                onClicked: root.controller.muted = !root.controller.muted
            }

            SonoraSlider {
                Layout.preferredWidth: root.compact ? 58 : 78
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
}
