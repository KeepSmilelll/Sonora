import QtQuick
import QtQuick.Layouts
import Sonora.UI

// Compact always-on-top frosted mini player pill (lazy-created via Loader, destroyed when closed)
Window {
    id: miniWin

    required property var controller
    signal restoreMainRequested()

    width: 348
    height: 86
    minimumWidth: 348
    maximumWidth: 348
    minimumHeight: 86
    maximumHeight: 86
    visible: true
    title: "Sonora Mini"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: DesignTokens.surfaceElevated
        border.width: 1
        border.color: DesignTokens.borderHighlight
        clip: true

        DragHandler {
            target: null
            onActiveChanged: if (active) miniWin.startSystemMove()
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Rectangle {
                implicitWidth: 56; implicitHeight: 56; radius: 11
                color: DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                clip: true

                Image {
                    anchors.fill: parent
                    source: miniWin.controller.trackCoverUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: status === Image.Ready
                }
                IconGlyph {
                    anchors.centerIn: parent; name: "music"; size: 22; color: DesignTokens.textTertiary
                    visible: miniWin.controller.trackCoverUrl.length === 0
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: miniWin.controller.hasTrack ? miniWin.controller.trackTitle : "Sonora"
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: miniWin.controller.trackArtist.length > 0 ? miniWin.controller.trackArtist : qsTr("Idle")
                    elide: Text.ElideRight
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                }
            }

            RowLayout {
                spacing: 4

                Rectangle {
                    implicitWidth: 28; implicitHeight: 28; radius: 14
                    color: pMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    IconGlyph { anchors.centerIn: parent; name: "prev"; size: 14; color: DesignTokens.textPrimary }
                    MouseArea { id: pMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: miniWin.controller.previous() }
                }
                Rectangle {
                    implicitWidth: 34; implicitHeight: 34; radius: 17
                    color: DesignTokens.accent
                    IconGlyph { anchors.centerIn: parent; name: miniWin.controller.isPlaying ? "pause" : "play"; size: 15; color: "#ffffff" }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: miniWin.controller.togglePlayPause() }
                }
                Rectangle {
                    implicitWidth: 28; implicitHeight: 28; radius: 14
                    color: nMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    IconGlyph { anchors.centerIn: parent; name: "next"; size: 14; color: DesignTokens.textPrimary }
                    MouseArea { id: nMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: miniWin.controller.next() }
                }
                Rectangle {
                    implicitWidth: 26; implicitHeight: 26; radius: 13
                    color: rMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    IconGlyph { anchors.centerIn: parent; name: "maximize"; size: 12; color: DesignTokens.textSecondary }
                    MouseArea { id: rMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: miniWin.restoreMainRequested() }
                }
            }
        }

        // Ultra-slim 2px progress line along bottom edge
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: DesignTokens.borderSubtle

            Rectangle {
                height: parent.height
                width: miniWin.controller.durationMs > 0
                    ? parent.width * Math.min(1.0, miniWin.controller.positionMs / miniWin.controller.durationMs)
                    : 0
                color: DesignTokens.accent
            }
        }
    }
}
