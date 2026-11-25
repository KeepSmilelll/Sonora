import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    required property var controller
    signal closeRequested()

    width: 320
    color: DesignTokens.surfaceElevated
    border.width: 1
    border.color: DesignTokens.borderSubtle
    radius: DesignTokens.radiusLg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Play Queue (%1)").arg(root.controller.queueCount)
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Rectangle {
                visible: root.controller.queueCount > 0
                implicitHeight: 24
                implicitWidth: clearLbl.implicitWidth + 16
                radius: 6
                color: clearMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                Text {
                    id: clearLbl
                    anchors.centerIn: parent
                    text: qsTr("Clear")
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
                MouseArea {
                    id: clearMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.controller.clearQueue()
                }
            }

            Rectangle {
                implicitWidth: 26; implicitHeight: 26; radius: 6
                color: closeQMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "close"; size: 13; color: DesignTokens.textSecondary }
                MouseArea {
                    id: closeQMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            reuseItems: true
            cacheBuffer: 200
            model: root.controller.queueModel

            delegate: Rectangle {
                id: qRow
                required property int index
                required property string title
                required property string artist
                required property int durationMs
                required property bool isCurrent

                width: ListView.view ? ListView.view.width : 280
                height: 46
                radius: 8
                color: isCurrent ? DesignTokens.surfaceSelected
                                 : (rowMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: qRow.title
                            elide: Text.ElideRight
                            color: qRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: qRow.isCurrent ? Font.DemiBold : Font.Normal
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

                    Text {
                        text: root.controller.formatTime(qRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        visible: !rowMa.containsMouse
                    }

                    Rectangle {
                        visible: rowMa.containsMouse
                        implicitWidth: 24; implicitHeight: 24; radius: 6
                        color: rmMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                        IconGlyph { anchors.centerIn: parent; name: "close"; size: 11; color: DesignTokens.textSecondary }
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
                    anchors.rightMargin: 32
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.controller.playQueueIndex(qRow.index)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.controller.queueCount === 0
                text: qsTr("Queue is empty\nDrop audio files anywhere to start listening")
                horizontalAlignment: Text.AlignHCenter
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 13
                lineHeight: 1.5
            }
        }
    }
}
