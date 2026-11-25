import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

// Immersive Apple Music-style Now Playing & Synced Lyrics view (lazy-loaded via Loader)
Item {
    id: root

    required property var controller
    required property var lyricsCtrl
    required property var settingsObj

    signal closeRequested()

    AmbientBackdrop {
        anchors.fill: parent
        immersive: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Top close / source / offset strip
        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: 34; implicitHeight: 34; radius: 17
                color: closeMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                IconGlyph { anchors.centerIn: parent; name: "chevron_down"; size: 17; color: DesignTokens.textPrimary }
                MouseArea {
                    id: closeMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }

            Item { Layout.fillWidth: true }

            // Lyric offset fine-tuning controls
            RowLayout {
                visible: root.lyricsCtrl.isSynced
                spacing: 6

                Rectangle {
                    implicitHeight: 26; implicitWidth: 46; radius: 13
                    color: mMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                    border.width: 1; border.color: DesignTokens.borderSubtle
                    Text {
                        anchors.centerIn: parent; text: "-0.25s"
                        color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 11
                    }
                    MouseArea {
                        id: mMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lyricsCtrl.adjustOffsetMs(-250)
                    }
                }

                Text {
                    text: (root.lyricsCtrl.userOffsetMs >= 0 ? "+" : "") + (root.lyricsCtrl.userOffsetMs / 1000.0).toFixed(2) + "s"
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                }

                Rectangle {
                    implicitHeight: 26; implicitWidth: 46; radius: 13
                    color: pMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                    border.width: 1; border.color: DesignTokens.borderSubtle
                    Text {
                        anchors.centerIn: parent; text: "+0.25s"
                        color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 11
                    }
                    MouseArea {
                        id: pMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lyricsCtrl.adjustOffsetMs(250)
                    }
                }
            }

            FormatBadge {
                visible: root.lyricsCtrl.sourceLabel.length > 0
                text: root.lyricsCtrl.sourceLabel
            }
        }

        // Main 2-column immersive layout
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 44

            // Left column: Album artwork + Track info + Live GPU Spectrum Visualizer
            ColumnLayout {
                Layout.preferredWidth: Math.min(380, root.width * 0.38)
                Layout.alignment: Qt.AlignVCenter
                spacing: 16

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: Math.min(340, parent.width)
                    implicitHeight: implicitWidth
                    radius: DesignTokens.radiusXl
                    color: DesignTokens.surfaceCard
                    border.width: 1
                    border.color: DesignTokens.borderHighlight
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: root.controller.trackCoverUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                        visible: status === Image.Ready
                    }

                    IconGlyph {
                        anchors.centerIn: parent
                        name: "music"
                        size: 72
                        color: DesignTokens.textTertiary
                        visible: root.controller.trackCoverUrl.length === 0
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: root.controller.trackTitle
                        elide: Text.ElideRight
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.controller.trackAlbum.length > 0
                            ? (root.controller.trackArtist + " — " + root.controller.trackAlbum)
                            : root.controller.trackArtist
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 14
                    }
                    RowLayout {
                        spacing: 6
                        Layout.topMargin: 4
                        FormatBadge { text: root.controller.trackFormatBadge }
                        FormatBadge {
                            visible: root.controller.outputStatusBadge.length > 0
                            text: root.controller.outputStatusBadge
                        }
                    }
                }

                // Single-draw-call 60fps GPU Spectrum Visualizer
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 64
                    radius: 12
                    color: DesignTokens.surfaceCard
                    border.width: 1
                    border.color: DesignTokens.borderSubtle
                    visible: root.settingsObj.visualizerEnabled

                    SpectrumVisualizerItem {
                        anchors.fill: parent
                        anchors.margins: 8
                        accentColor: DesignTokens.accent
                        secondaryColor: DesignTokens.ambientPrimary.lighter(170)
                        playing: root.controller.isPlaying
                        visualizerEnabled: root.settingsObj.visualizerEnabled
                    }
                }
            }

            // Right column: Smooth auto-centering synced lyrics
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: lyricsList
                    anchors.fill: parent
                    clip: true
                    spacing: 14
                    model: root.lyricsCtrl
                    preferredHighlightBegin: height * 0.36
                    preferredHighlightEnd: height * 0.44
                    highlightRangeMode: userScrolling ? ListView.NoHighlightRange : ListView.StrictlyEnforceRange
                    highlightMoveDuration: 360
                    currentIndex: root.lyricsCtrl.activeLineIndex
                    visible: root.lyricsCtrl.hasLyrics

                    property bool userScrolling: false
                    onMovementStarted: {
                        userScrolling = true
                        resumeFollowTimer.restart()
                    }

                    Timer {
                        id: resumeFollowTimer
                        interval: 3200
                        onTriggered: lyricsList.userScrolling = false
                    }

                    header: Item { width: 1; height: lyricsList.height * 0.32 }
                    footer: Item { width: 1; height: lyricsList.height * 0.45 }

                    delegate: Item {
                        id: lyrRow
                        required property int index
                        required property int timestampMs
                        required property string text
                        required property string translation
                        required property bool isActive

                        width: lyricsList.width
                        height: col.implicitHeight + 12

                        ColumnLayout {
                            id: col
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: lyrRow.text.length > 0 ? lyrRow.text : "· · ·"
                                color: lyrRow.isActive ? DesignTokens.textPrimary : DesignTokens.textSecondary
                                opacity: lyrRow.isActive ? 1.0 : (lyrMa.containsMouse ? 0.78 : 0.42)
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: lyrRow.isActive ? 24 : 18
                                font.weight: lyrRow.isActive ? Font.Bold : Font.Medium
                                Behavior on opacity { NumberAnimation { duration: 220 } }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: lyrRow.translation.length > 0
                                wrapMode: Text.WordWrap
                                text: lyrRow.translation
                                color: lyrRow.isActive ? DesignTokens.textSecondary : DesignTokens.textTertiary
                                opacity: lyrRow.isActive ? 0.92 : 0.38
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: lyrRow.isActive ? 15 : 13
                                Behavior on opacity { NumberAnimation { duration: 220 } }
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

                ColumnLayout {
                    anchors.centerIn: parent
                    visible: !root.lyricsCtrl.hasLyrics
                    spacing: 10
                    IconGlyph {
                        Layout.alignment: Qt.AlignHCenter
                        name: "lyrics"; size: 36; color: DesignTokens.textTertiary
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("No Lyrics Available")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Place a matching .lrc file next to the audio file or embed USLT/LYRICS tags.")
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
