import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Sonora.UI

// Compact always-on-top Spatial Fluid Glass mini player capsule (created with null parent so tray hide never hides it)
Window {
    id: miniWin

    required property var controller
    signal restoreMainRequested()

    width: 372
    height: 94
    minimumWidth: 372
    maximumWidth: 372
    minimumHeight: 94
    maximumHeight: 94
    visible: true
    transientParent: null
    title: "Sonora Mini"
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
        anchors.fill: parent
        radius: DesignTokens.radiusLg
        panelColor: DesignTokens.playbackSurface
        borderColor: DesignTokens.borderOuter
        specularEnabled: false
        shadowEnabled: false

        Item {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: seekArea.top

            DragHandler {
                target: null
                onActiveChanged: if (active) miniWin.startSystemMove()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: seekArea.height
            spacing: 9

            Rectangle {
                implicitWidth: 48
                implicitHeight: 48
                radius: DesignTokens.radiusSm
                color: DesignTokens.surfaceCard
                border.width: 1
                border.color: DesignTokens.borderHighlight
                clip: true

                Rectangle {
                    id: miniCoverMask
                    anchors.fill: parent
                    radius: DesignTokens.radiusSm
                    visible: false
                    layer.enabled: true
                    layer.smooth: true
                }

                Image {
                    id: miniCoverImg
                    anchors.fill: parent
                    source: miniWin.controller.trackCoverUrl
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
                    size: 22
                    color: DesignTokens.textTertiary
                    visible: miniCoverImg.status !== Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: miniWin.controller.hasTrack ? miniWin.controller.trackTitle : "Sonora"
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: miniWin.controller.trackArtist.length > 0 ? miniWin.controller.trackArtist : qsTr("Ready to play")
                    elide: Text.ElideRight
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                }
            }

            RowLayout {
                spacing: 4

                MiniIconBtn {
                    iconName: "prev"
                    iconSize: 14
                    onClicked: miniWin.controller.previous()
                }

                Item {
                    implicitWidth: 34
                    implicitHeight: 34
                    scale: miniPlayMa.pressed ? 0.90 : (miniPlayMa.containsMouse ? 1.08 : 1.0)
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: miniPlayMa.pressed ? Qt.darker(DesignTokens.accent, 1.15) : DesignTokens.accent

                        IconGlyph {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: miniWin.controller.isPlaying ? 0 : 1
                            name: miniWin.controller.isPlaying ? "pause" : "play"
                            size: 17
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
                    iconName: "next"
                    iconSize: 14
                    onClicked: miniWin.controller.next()
                }

                MiniIconBtn {
                    iconName: "maximize"
                    iconSize: 12
                    onClicked: miniWin.restoreMainRequested()
                }
            }
        }

        MouseArea {
            id: seekArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            height: 20
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
                width: 9
                height: 9
                radius: height / 2
                color: DesignTokens.textPrimary
                border.width: 1
                border.color: DesignTokens.accent
                opacity: seekArea.interacting ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
        }
    }
}
