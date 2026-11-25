import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib
    signal addFolderRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                visible: root.lib.canGoUpFolder
                implicitWidth: 30; implicitHeight: 30; radius: 8
                color: upMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                IconGlyph { anchors.centerIn: parent; name: "chevron_left"; size: 15; color: DesignTokens.textPrimary }
                MouseArea {
                    id: upMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.goUpFolder()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Folders")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    text: root.lib.currentFolderPath.length > 0 ? root.lib.currentFolderPath : qsTr("Watched Music Directories")
                    elide: Text.ElideMiddle
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }

            Rectangle {
                implicitHeight: 32; implicitWidth: addFLbl.implicitWidth + 24; radius: 16
                color: addFMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                border.width: 1; border.color: DesignTokens.borderSubtle
                Text {
                    id: addFLbl; anchors.centerIn: parent
                    text: qsTr("+ Add Folder")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
                MouseArea {
                    id: addFMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.addFolderRequested()
                }
            }
        }

        // Subdirectories strip
        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: root.lib.folderEntries.length > 0

            Repeater {
                model: root.lib.folderEntries
                delegate: Rectangle {
                    id: fCard
                    required property var modelData
                    height: 36
                    width: Math.min(260, fRow.implicitWidth + 24)
                    radius: 10
                    color: fMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceCard
                    border.width: 1; border.color: DesignTokens.borderSubtle

                    RowLayout {
                        id: fRow
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 10
                        spacing: 8
                        IconGlyph { name: "folder"; size: 15; color: DesignTokens.accent }
                        Text {
                            Layout.fillWidth: true
                            text: fCard.modelData.name
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                        }
                    }
                    MouseArea {
                        id: fMa; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.lib.openFolder(fCard.modelData.path)
                    }
                }
            }
        }

        // Tracks in current folder
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            reuseItems: true
            model: root.lib.detailTracksModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: ftRow
                required property int index
                required property int trackId
                required property string title
                required property string artist
                required property int durationMs
                required property string formatBadge

                readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                width: ListView.view ? ListView.view.width : 500
                height: 44
                radius: 8
                color: isCurrent ? DesignTokens.surfaceSelected
                                 : (ftMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: ftRow.title
                        elide: Text.ElideRight
                        color: ftRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: ftRow.artist
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                    FormatBadge { compact: true; text: ftRow.formatBadge }
                    Text {
                        text: root.controller.formatTime(ftRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    id: ftMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromDetailModel(ftRow.index)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.lib.folderEntries.length === 0 && root.lib.detailTracksModel.count === 0
                text: qsTr("Add local music folders to browse by directory structure")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 13
            }
        }
    }
}
