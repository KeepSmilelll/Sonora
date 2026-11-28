import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var lib
    property bool embedded: false
    signal addFolderRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.embedded ? 0 : (root.width < 680 ? 16 : (root.width >= 1560 ? 26 : 22))
        anchors.topMargin: root.embedded ? 4 : anchors.margins
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            VirelaButton {
                visible: root.lib.canGoUpFolder
                compact: true
                iconName: "chevron_left"
                onClicked: root.lib.goUpFolder()
            }

            Text {
                visible: !root.embedded
                text: qsTr("Folders")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 24
                font.weight: Font.Bold
                font.letterSpacing: -0.3
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.maximumWidth: Math.min(760, Math.max(220, Math.round(root.width * 0.56)))
                implicitHeight: 28
                radius: 14
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: DesignTokens.hairlineBorder

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    text: root.lib.currentFolderPath.length > 0 ? root.lib.currentFolderPath : qsTr("Watched Music Directories")
                    elide: Text.ElideMiddle
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            VirelaButton {
                accentTinted: true
                iconName: "plus"
                text: qsTr("Add Folder")
                onClicked: root.addFolderRequested()
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
                    height: 38
                    width: Math.min(280, fRow.implicitWidth + 26)
                    radius: 12
                    color: fMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                    border.width: 1
                    border.color: fMa.containsMouse
                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.45)
                        : DesignTokens.hairlineBorder
                    scale: fMa.pressed ? 0.96 : (fMa.containsMouse ? 1.02 : 1.0)

                    Behavior on color { ColorAnimation { duration: 130 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                    RowLayout {
                        id: fRow
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 11
                        spacing: 8

                        IconGlyph { name: "folder"; size: 15; color: DesignTokens.accent }
                        Text {
                            Layout.fillWidth: true
                            text: fCard.modelData.name
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        IconGlyph { name: "chevron_right"; size: 12; color: DesignTokens.textTertiary }
                    }

                    MouseArea {
                        id: fMa
                        anchors.fill: parent
                        hoverEnabled: true
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
            spacing: 3
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
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
                height: 46
                radius: 10
                color: isCurrent
                    ? DesignTokens.surfaceSelected
                    : (ftMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                border.width: isCurrent ? 1 : 0
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                Behavior on color { ColorAnimation { duration: 110 } }

                Rectangle {
                    visible: ftRow.isCurrent
                    width: 3
                    height: 18
                    radius: 1.5
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: DesignTokens.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 14
                    spacing: 12

                    Item {
                        Layout.preferredWidth: 24
                        implicitHeight: 20

                        NowPlayingBars {
                            anchors.centerIn: parent
                            visible: ftRow.isCurrent
                            controller: root.controller
                            size: 13
                        }

                        IconGlyph {
                            anchors.centerIn: parent
                            visible: !ftRow.isCurrent
                            name: "music"
                            size: 13
                            color: DesignTokens.textTertiary
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: ftRow.title
                        elide: Text.ElideRight
                        color: ftRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: ftRow.isCurrent ? Font.DemiBold : Font.Medium
                    }

                    Text {
                        visible: root.width >= 540
                        Layout.preferredWidth: Math.min(260, Math.max(110, Math.round(root.width * 0.20)))
                        text: ftRow.artist
                        elide: Text.ElideRight
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                    }

                    FormatBadge { compact: true; text: ftRow.formatBadge }

                    Text {
                        Layout.preferredWidth: 48
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(ftRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.features: ({ "tnum": 1 })
                    }
                }

                MouseArea {
                    id: ftMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromDetailModel(ftRow.index)
                }
            }

            GlassPanel {
                anchors.centerIn: parent
                visible: root.lib.folderEntries.length === 0 && root.lib.detailTracksModel.count === 0
                width: Math.min(parent.width - 64, 420)
                height: emptyFolderCol.implicitHeight + 44
                radius: DesignTokens.radiusXl
                panelColor: DesignTokens.stageSurfaceBg
                borderColor: DesignTokens.hairlineBorder
                shadowEnabled: false

                ColumnLayout {
                    id: emptyFolderCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 10

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 54; implicitHeight: 54; radius: 16
                        color: DesignTokens.accentSoft
                        IconGlyph { anchors.centerIn: parent; name: "folder"; size: 24; color: DesignTokens.accent }
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Add local music folders to browse directly by directory structure")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 4
                        primary: true
                        iconName: "folder"
                        text: qsTr("Add Music Folder")
                        onClicked: root.addFolderRequested()
                    }
                }
            }
        }
    }
}
