import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    required property var controller
    property bool lyricsOpen: false
    property bool eqOpen: false
    property bool queueOpen: false

    signal toggleLyrics()
    signal toggleEq()
    signal toggleQueue()
    signal openTrackInfo()
    signal toggleFavorite()

    implicitHeight: DesignTokens.playbackBarHeight
    color: DesignTokens.playbackBarBg

    // Crisp top specular hairline
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: DesignTokens.borderSubtle
    }

    component IconBtn: Rectangle {
        id: btn
        property string iconName: "play"
        property bool active: false
        property bool primary: false
        property real iconSize: 16
        signal clicked()

        implicitWidth: primary ? 38 : 32
        implicitHeight: primary ? 38 : 32
        radius: implicitHeight / 2
        color: {
            if (primary) return ma.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent
            if (active) return DesignTokens.surfaceSelected
            if (ma.pressed) return DesignTokens.surfacePressed
            if (ma.containsMouse) return DesignTokens.surfaceHover
            return "transparent"
        }

        IconGlyph {
            anchors.centerIn: parent
            name: btn.iconName
            size: btn.iconSize
            color: btn.primary ? "#ffffff"
                               : (btn.active ? DesignTokens.accent : DesignTokens.textPrimary)
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        // Left: Cover + Track metadata + Hi-Res / Output badge
        RowLayout {
            Layout.preferredWidth: 280
            Layout.maximumWidth: 320
            spacing: 12

            Rectangle {
                id: coverCard
                implicitWidth: 54
                implicitHeight: 54
                radius: 10
                color: DesignTokens.surfaceCard
                border.width: 1
                border.color: DesignTokens.borderSubtle
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
                    size: 22
                    color: DesignTokens.textTertiary
                    visible: root.controller.trackCoverUrl.length === 0
                }

                // Hover expand hint to open immersive lyrics/now-playing view
                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.35)
                    opacity: coverMa.containsMouse && root.controller.hasTrack ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 140 } }

                    IconGlyph {
                        anchors.centerIn: parent
                        name: "lyrics"
                        size: 18
                        color: "#ffffff"
                    }
                }

                MouseArea {
                    id: coverMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.controller.hasTrack ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: if (root.controller.hasTrack) root.toggleLyrics()
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

                    IconBtn {
                        visible: root.controller.hasTrack && root.controller.trackId >= 0
                        implicitWidth: 24; implicitHeight: 24; iconSize: 14
                        iconName: root.controller.trackIsFavorite ? "heart_filled" : "heart"
                        active: root.controller.trackIsFavorite
                        onClicked: root.toggleFavorite()
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

                RowLayout {
                    spacing: 6
                    visible: root.controller.hasTrack

                    FormatBadge {
                        compact: true
                        text: root.controller.trackFormatBadge
                    }

                    FormatBadge {
                        compact: true
                        visible: root.controller.exclusiveActive
                        text: root.controller.bitPerfectActive ? qsTr("Bit-Perfect") : qsTr("Exclusive")
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.controller.trackTechSummary
                        elide: Text.ElideRight
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Center: Transport buttons + sleek seek bar with hover time tooltip
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                IconBtn {
                    iconName: "shuffle"
                    iconSize: 15
                    active: root.controller.shuffle
                    onClicked: root.controller.shuffle = !root.controller.shuffle
                }
                IconBtn {
                    iconName: "prev"
                    iconSize: 17
                    onClicked: root.controller.previous()
                }
                IconBtn {
                    primary: true
                    iconName: root.controller.isPlaying ? "pause" : "play"
                    iconSize: 17
                    onClicked: root.controller.togglePlayPause()
                }
                IconBtn {
                    iconName: "next"
                    iconSize: 17
                    onClicked: root.controller.next()
                }
                IconBtn {
                    iconName: root.controller.repeatMode === 2 ? "repeat_one" : "repeat"
                    iconSize: 15
                    active: root.controller.repeatMode !== 0
                    onClicked: root.controller.cycleRepeatMode()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 540
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    text: root.controller.formatTime(seekMa.pressed ? seekMa.hoverRatio * root.controller.durationMs : root.controller.positionMs)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 38
                    horizontalAlignment: Text.AlignRight
                }

                Item {
                    id: seekTrack
                    Layout.fillWidth: true
                    implicitHeight: 18

                    readonly property real ratio: root.controller.durationMs > 0
                        ? Math.max(0.0, Math.min(1.0, (seekMa.pressed ? seekMa.hoverRatio : (root.controller.positionMs / root.controller.durationMs))))
                        : 0.0

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: seekMa.containsMouse || seekMa.pressed ? 5 : 4
                        radius: height / 2
                        color: DesignTokens.isDark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.10)

                        Rectangle {
                            width: parent.width * seekTrack.ratio
                            height: parent.height
                            radius: parent.radius
                            color: DesignTokens.accent
                        }
                    }

                    Rectangle {
                        visible: seekMa.containsMouse || seekMa.pressed
                        width: 12; height: 12; radius: 6
                        anchors.verticalCenter: parent.verticalCenter
                        x: Math.max(0, Math.min(parent.width - width, parent.width * seekTrack.ratio - width / 2))
                        color: "#ffffff"
                        border.width: 2
                        border.color: DesignTokens.accent
                    }

                    MouseArea {
                        id: seekMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        property real hoverRatio: 0.0

                        function updateRatio(mx) {
                            hoverRatio = Math.max(0.0, Math.min(1.0, mx / Math.max(1, width)))
                        }
                        onPositionChanged: (mouse) => updateRatio(mouse.x)
                        onPressed: (mouse) => updateRatio(mouse.x)
                        onReleased: (mouse) => {
                            updateRatio(mouse.x)
                            root.controller.seekRatio(hoverRatio)
                        }
                    }
                }

                Text {
                    text: root.controller.formatTime(root.controller.durationMs)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.features: ({ "tnum": 1 })
                    Layout.preferredWidth: 38
                }
            }
        }

        // Right: Lyrics, EQ, Info, Queue, Volume
        RowLayout {
            Layout.preferredWidth: 260
            Layout.alignment: Qt.AlignRight
            spacing: 4

            IconBtn {
                iconName: "lyrics"
                iconSize: 15
                active: root.lyricsOpen
                onClicked: root.toggleLyrics()
            }
            IconBtn {
                iconName: "eq"
                iconSize: 15
                active: root.eqOpen
                onClicked: root.toggleEq()
            }
            IconBtn {
                iconName: "info"
                iconSize: 15
                visible: root.controller.hasTrack
                onClicked: root.openTrackInfo()
            }
            IconBtn {
                iconName: "queue"
                iconSize: 15
                active: root.queueOpen
                onClicked: root.toggleQueue()
            }

            IconBtn {
                iconName: (root.controller.muted || root.controller.volume <= 0.001) ? "volume_mute" : "volume"
                iconSize: 15
                onClicked: root.controller.muted = !root.controller.muted
            }

            Item {
                id: volTrack
                Layout.preferredWidth: 84
                implicitHeight: 18

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 4
                    radius: 2
                    color: DesignTokens.isDark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.10)

                    Rectangle {
                        width: parent.width * (root.controller.muted ? 0.0 : root.controller.volume)
                        height: parent.height
                        radius: 2
                        color: DesignTokens.textPrimary
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    function applyVol(mx) {
                        const v = Math.max(0.0, Math.min(1.0, mx / Math.max(1, width)))
                        root.controller.muted = false
                        root.controller.volume = v
                    }
                    onPressed: (mouse) => applyVol(mouse.x)
                    onPositionChanged: (mouse) => { if (pressed) applyVol(mouse.x) }
                }
            }
        }
    }
}
