import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

GlassPanel {
    id: root

    required property var controller
    signal closeRequested()

    implicitWidth: 282
    radius: DesignTokens.radiusLg
    panelColor: DesignTokens.queueSurface
    borderColor: DesignTokens.borderOuter
    specularEnabled: false
    shadowEnabled: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 13
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.minimumWidth: 0
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: qsTr("Play Queue")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                text: qsTr("%1 track(s)").arg(root.controller.queueCount)
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
            }

            SonoraButton {
                visible: root.controller.queueCount > 0
                compact: true
                text: qsTr("Clear")
                onClicked: root.controller.clearQueue()
            }

            Rectangle {
                implicitWidth: 28
                implicitHeight: 28
                radius: 8
                color: closeQMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    name: "close"
                    size: 13
                    color: closeQMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                }
                MouseArea {
                    id: closeQMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: DesignTokens.borderSubtle
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 5
            reuseItems: true
            cacheBuffer: 200
            boundsBehavior: Flickable.StopAtBounds
            model: root.controller.queueModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: qRow
                required property int index
                required property string title
                required property string artist
                required property string coverHash
                required property int durationMs
                required property bool isCurrent

                readonly property bool hovered: rowMa.containsMouse || rmMa.containsMouse
                readonly property bool nextStartsHere: root.controller.currentIndex >= 0
                    && index === root.controller.currentIndex + 1
                width: ListView.view ? ListView.view.width : 256
                height: 47 + (groupLabel.visible ? 21 : 0)

                Text {
                    id: groupLabel
                    visible: qRow.isCurrent || qRow.nextStartsHere
                    height: visible ? 21 : 0
                    verticalAlignment: Text.AlignVCenter
                    text: qRow.isCurrent ? qsTr("Now Playing") : qsTr("Up Next")
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }

                Rectangle {
                    anchors.top: groupLabel.bottom
                    anchors.bottom: parent.bottom
                    width: parent.width
                    radius: DesignTokens.radiusSm
                    color: qRow.isCurrent ? DesignTokens.surfaceSelected
                        : (qRow.hovered ? DesignTokens.surfaceHover : "transparent")
                    Behavior on color { ColorAnimation { duration: 110 } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.topMargin: groupLabel.height
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 9

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: DesignTokens.radiusSm
                        color: DesignTokens.librarySurface
                        clip: true

                        IconGlyph {
                            anchors.centerIn: parent
                            visible: queueCover.status !== Image.Ready
                            name: "music"
                            size: 16
                            color: DesignTokens.textTertiary
                        }

                        Image {
                            id: queueCover
                            anchors.fill: parent
                            source: qRow.isCurrent ? root.controller.trackCoverUrl
                                : (qRow.coverHash.length > 0 ? "image://cover/" + qRow.coverHash + "?size=96" : "")
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
                            text: qRow.title
                            elide: Text.ElideRight
                            color: qRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: qRow.isCurrent ? Font.DemiBold : Font.Medium
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qRow.artist.length > 0 ? qRow.artist : qsTr("Unknown Artist")
                            elide: Text.ElideRight
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }

                    NowPlayingBars {
                        visible: qRow.isCurrent && !qRow.hovered
                        playing: root.controller.isPlaying
                        size: 15
                    }

                    Text {
                        text: root.controller.formatTime(qRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.features: ({ "tnum": 1 })
                        visible: !qRow.isCurrent && !qRow.hovered
                    }

                    Rectangle {
                        visible: qRow.hovered
                        implicitWidth: 26
                        implicitHeight: 26
                        radius: 7
                        color: rmMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                        IconGlyph {
                            anchors.centerIn: parent
                            name: "close"
                            size: 11
                            color: rmMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary
                        }
                        MouseArea {
                            id: rmMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.controller.removeFromQueue(qRow.index)
                        }
                    }
                }

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    anchors.topMargin: groupLabel.height
                    anchors.rightMargin: 34
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.controller.playQueueIndex(qRow.index)
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: root.controller.queueCount === 0
                spacing: 8

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 46; implicitHeight: 46; radius: 14
                    color: DesignTokens.accentSoft
                    IconGlyph { anchors.centerIn: parent; name: "queue"; size: 20; color: DesignTokens.accent }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Queue is empty\nDrop audio files anywhere to start listening")
                    horizontalAlignment: Text.AlignHCenter
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                    lineHeight: 1.45
                }
            }
        }
    }
}
