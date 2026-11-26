import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Sonora.UI

// Now-playing artwork and synchronized lyrics.
GlassPanel {
    id: root

    required property var controller
    required property var lyricsCtrl
    required property var settingsObj

    signal closeRequested()
    signal toggleFavorite()

    radius: DesignTokens.radiusLg
    panelColor: DesignTokens.solidWindowBg
    borderColor: "transparent"
    shadowEnabled: false
    specularEnabled: false

    readonly property real leftColWidth: Math.round(Math.max(235, Math.min(410, root.width * 0.35)))

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 1
        spacing: 12

        // Return, lyric offset, and current-track controls.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 39
            spacing: 10

            Rectangle {
                implicitWidth: 30
                implicitHeight: 30
                radius: DesignTokens.radiusSm
                color: closeMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                scale: closeMa.pressed ? 0.90 : (closeMa.containsMouse ? 1.06 : 1.0)
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                Behavior on color { ColorAnimation { duration: 130 } }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "chevron_down"
                    size: 17
                    color: DesignTokens.textPrimary
                }
                MouseArea {
                    id: closeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }

            Text {
                text: qsTr("Now Playing")
                color: DesignTokens.textSecondary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 14
                font.weight: Font.Medium
            }

            Item { Layout.fillWidth: true }

            // Lyric offset fine-tuning controls
            RowLayout {
                visible: root.lyricsCtrl.isSynced
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
                visible: root.lyricsCtrl.sourceLabel.length > 0
                text: root.lyricsCtrl.sourceLabel
            }

            SonoraButton {
                compact: true
                flat: true
                accentTinted: root.controller.trackIsFavorite
                visible: root.controller.trackId >= 0
                iconName: root.controller.trackIsFavorite ? "heart_filled" : "heart"
                onClicked: root.toggleFavorite()
            }
        }

        // Main 2-column immersive layout
        RowLayout {
            id: stageRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            readonly property real artSize: Math.round(
                Math.max(144, Math.min(314, root.leftColWidth - 24, stageRow.height - 148))
            )

            // Left column: strictly width-clamped, vertically centered compact group
            ColumnLayout {
                id: leftCol
                Layout.preferredWidth: root.leftColWidth
                Layout.minimumWidth: root.leftColWidth
                Layout.maximumWidth: root.leftColWidth
                Layout.fillHeight: false
                Layout.maximumHeight: stageRow.height
                Layout.alignment: Qt.AlignVCenter
                spacing: 14

                // Album artwork.
                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: stageRow.artSize
                    Layout.preferredHeight: stageRow.artSize
                    Layout.maximumWidth: stageRow.artSize
                    Layout.maximumHeight: stageRow.artSize

                    Rectangle {
                        id: artFrame
                        anchors.fill: parent
                        radius: DesignTokens.radiusXl
                        color: DesignTokens.surfaceCard
                        border.width: 1
                        border.color: DesignTokens.borderHighlight
                        clip: true

                        Rectangle {
                            id: artMask
                            anchors.fill: parent
                            radius: DesignTokens.radiusXl
                            visible: false
                            layer.enabled: true
                            layer.smooth: true
                        }

                        Image {
                            id: artImg
                            anchors.fill: parent
                            source: root.controller.trackCoverUrl
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            visible: false
                        }

                        MultiEffect {
                            anchors.fill: artImg
                            visible: artImg.status === Image.Ready
                            source: artImg
                            maskEnabled: true
                            maskSource: artMask
                            maskThresholdMin: 0.5
                            maskSpreadAtMin: 1.0
                        }

                        IconGlyph {
                            anchors.centerIn: parent
                            name: "music"
                            size: Math.min(72, stageRow.artSize * 0.28)
                            color: DesignTokens.textTertiary
                            visible: artImg.status !== Image.Ready
                        }

                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: root.leftColWidth
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: root.leftColWidth
                        text: root.controller.trackTitle
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 21
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: root.leftColWidth
                        text: root.controller.trackAlbum.length > 0
                            ? (root.controller.trackArtist + " — " + root.controller.trackAlbum)
                            : root.controller.trackArtist
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: false
                        Layout.maximumWidth: root.leftColWidth
                        Layout.topMargin: 4
                        spacing: 6
                        FormatBadge {
                            visible: root.controller.trackFormatBadge.length > 0
                            text: root.controller.trackFormatBadge
                        }
                        FormatBadge {
                            visible: root.controller.outputStatusBadge.length > 0
                            text: root.controller.outputStatusBadge
                        }
                    }
                }

                // Single-draw-call 60fps GPU Spectrum Visualizer
                GlassPanel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 28
                    panelColor: "transparent"
                    borderColor: "transparent"
                    specularEnabled: false
                    shadowEnabled: false
                    visible: root.settingsObj.visualizerEnabled

                    SpectrumVisualizerItem {
                        anchors.fill: parent
                        accentColor: DesignTokens.accent
                        secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                        playing: root.controller.isPlaying
                        visualizerEnabled: root.settingsObj.visualizerEnabled
                    }
                }
            }

            // Right column: strictly clipped viewport with auto-centering synced lyrics
            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 260
                radius: DesignTokens.radiusLg
                panelColor: DesignTokens.librarySurface
                borderColor: DesignTokens.borderSubtle
                specularEnabled: false

                RowLayout {
                    id: lyricsHeader
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 15
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    height: 34
                    spacing: 8

                    IconGlyph { name: "lyrics"; size: 17; color: DesignTokens.accentLight }
                    Text {
                        Layout.fillWidth: true
                        text: root.lyricsCtrl.isSynced ? qsTr("Synced Lyrics") : qsTr("Lyrics")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    Text {
                        text: root.controller.formatTime(root.controller.positionMs)
                            + "  /  " + root.controller.formatTime(root.controller.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.features: ({ "tnum": 1 })
                    }
                }

                Rectangle {
                    id: lyricsRule
                    anchors.top: lyricsHeader.bottom
                    anchors.topMargin: 9
                    anchors.left: lyricsHeader.left
                    anchors.right: lyricsHeader.right
                    height: 1
                    color: DesignTokens.borderSubtle
                }

                ListView {
                    id: lyricsList
                    anchors.top: lyricsRule.bottom
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 24
                    anchors.rightMargin: 12
                    clip: true
                    spacing: 5
                    topMargin: Math.round(height * 0.38)
                    bottomMargin: Math.round(height * 0.55)
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.lyricsCtrl
                    preferredHighlightBegin: Math.round(height * 0.38)
                    preferredHighlightEnd: Math.round(height * 0.46)
                    highlightRangeMode: userScrolling ? ListView.NoHighlightRange : ListView.StrictlyEnforceRange
                    highlightMoveDuration: 340
                    currentIndex: root.lyricsCtrl.activeLineIndex >= 0 ? root.lyricsCtrl.activeLineIndex : 0
                    visible: root.lyricsCtrl.hasLyrics
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    property bool userScrolling: false

                    function recenterActiveLine(immediate) {
                        if (!root.lyricsCtrl.hasLyrics || count <= 0) return
                        const idx = root.lyricsCtrl.activeLineIndex >= 0 ? root.lyricsCtrl.activeLineIndex : 0
                        if (immediate) {
                            positionViewAtIndex(idx, ListView.Center)
                        }
                    }

                    Component.onCompleted: Qt.callLater(function() { recenterActiveLine(true) })
                    onHeightChanged: {
                        if (!userScrolling) {
                            Qt.callLater(function() { recenterActiveLine(true) })
                        }
                    }
                    onCountChanged: Qt.callLater(function() { recenterActiveLine(true) })

                    onMovementStarted: {
                        userScrolling = true
                        resumeFollowTimer.restart()
                    }

                    Timer {
                        id: resumeFollowTimer
                        interval: 3200
                        onTriggered: lyricsList.userScrolling = false
                    }

                    delegate: Item {
                        id: lyrRow
                        required property int index
                        required property int timestampMs
                        required property string text
                        required property string translation
                        required property bool isActive

                        width: lyricsList.width - 12
                        height: col.implicitHeight + 12

                        // Hover pill feedback for click-to-seek
                        Rectangle {
                            anchors.fill: parent
                            radius: 12
                            color: (lyrMa.containsMouse && root.lyricsCtrl.isSynced)
                                ? DesignTokens.surfaceHover
                                : "transparent"
                            Behavior on color { ColorAnimation { duration: 130 } }

                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                visible: lyrMa.containsMouse && root.lyricsCtrl.isSynced && lyrRow.timestampMs >= 0
                                text: root.controller.formatTime(lyrRow.timestampMs)
                                color: DesignTokens.accent
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                font.features: ({ "tnum": 1 })
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: Math.min(32, parent.height - 8)
                            radius: 1.5
                            color: DesignTokens.accent
                            visible: lyrRow.isActive
                        }

                        Column {
                            id: col
                            anchors.left: parent.left
                            anchors.leftMargin: lyrRow.isActive ? 15 : 0
                            anchors.right: parent.right
                            anchors.rightMargin: (lyrMa.containsMouse && root.lyricsCtrl.isSynced) ? 54 : 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Text {
                                width: parent.width
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                text: lyrRow.text.length > 0 ? lyrRow.text : "· · ·"
                                color: lyrRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                opacity: lyrRow.isActive || lyrMa.containsMouse ? 1.0 : 0.75
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: lyrRow.isActive ? (root.width < 990 ? 25 : 30) : 21
                                font.weight: lyrRow.isActive ? Font.Bold : Font.Medium
                                Behavior on opacity { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation { duration: 220 } }
                            }

                            Text {
                                width: parent.width
                                visible: lyrRow.translation.length > 0
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                text: lyrRow.translation
                                color: lyrRow.isActive ? DesignTokens.textSecondary : DesignTokens.textTertiary
                                opacity: lyrRow.isActive ? 0.94 : (lyrMa.containsMouse ? 0.72 : 0.38)
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                Behavior on opacity { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
                            }
                        }

                        MouseArea {
                            id: lyrMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: root.lyricsCtrl.isSynced ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.lyricsCtrl.isSynced) {
                                    const ts = root.lyricsCtrl.timestampForLine(lyrRow.index)
                                    if (ts >= 0) {
                                        lyricsList.userScrolling = false
                                        root.controller.seekMs(ts)
                                    }
                                }
                            }
                        }
                    }
                }

                // Empty lyrics state
                GlassPanel {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 370)
                    height: noLyrCol.implicitHeight + 44
                    radius: DesignTokens.radiusXl
                    shadowEnabled: false
                    visible: !root.lyricsCtrl.hasLyrics

                    ColumnLayout {
                        id: noLyrCol
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
            }
        }
    }
}
