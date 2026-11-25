import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib

    property string selectedArtist: ""

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Left Artist List
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            radius: DesignTokens.radiusLg
            color: DesignTokens.surfaceCard
            border.width: 1
            border.color: DesignTokens.borderSubtle

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: qsTr("Artists (%1)").arg(root.lib.artistsModel.count)
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    Layout.leftMargin: 6
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    reuseItems: true
                    model: root.lib.artistsModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        id: arRow
                        required property string artist
                        required property int albumCount
                        required property int trackCount

                        readonly property bool sel: root.selectedArtist === artist
                        width: ListView.view ? ListView.view.width : 210
                        height: 44
                        radius: 8
                        color: sel ? DesignTokens.surfaceSelected
                                   : (arMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10; anchors.rightMargin: 10
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: arRow.artist
                                elide: Text.ElideRight
                                color: arRow.sel ? DesignTokens.accent : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: arRow.sel ? Font.DemiBold : Font.Normal
                            }
                            Text {
                                text: qsTr("%1 album(s) · %2 track(s)").arg(arRow.albumCount).arg(arRow.trackCount)
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        MouseArea {
                            id: arMa; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedArtist = arRow.artist
                                root.lib.openArtist(arRow.artist)
                            }
                        }
                    }
                }
            }
        }

        // Right Artist Tracks Pane
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: root.selectedArtist.length > 0 ? root.selectedArtist : qsTr("Select an Artist")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Rectangle {
                    visible: root.selectedArtist.length > 0 && root.lib.detailTracksModel.count > 0
                    implicitHeight: 32; implicitWidth: pAllLbl.implicitWidth + 24; radius: 16
                    color: DesignTokens.accent
                    Text {
                        id: pAllLbl; anchors.centerIn: parent
                        text: qsTr("Play All")
                        color: "#ffffff"
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.playFromDetailModel(0)
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                reuseItems: true
                model: root.lib.detailTracksModel
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: atRow
                    required property int index
                    required property int trackId
                    required property string title
                    required property string album
                    required property int durationMs
                    required property string formatBadge

                    readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                    width: ListView.view ? ListView.view.width : 400
                    height: 44
                    radius: 8
                    color: isCurrent ? DesignTokens.surfaceSelected
                                     : (atMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 12
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: atRow.title
                                elide: Text.ElideRight
                                color: atRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: atRow.isCurrent ? Font.DemiBold : Font.Normal
                            }
                            Text {
                                Layout.fillWidth: true
                                text: atRow.album
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        FormatBadge { compact: true; text: atRow.formatBadge }
                        Text {
                            text: root.controller.formatTime(atRow.durationMs)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                        }
                    }
                    MouseArea {
                        id: atMa; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.playFromDetailModel(atRow.index)
                    }
                }
            }
        }
    }
}
