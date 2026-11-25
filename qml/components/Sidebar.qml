import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    property string activeSection: "tracks"
    property var customPlaylists: []
    property int activePlaylistId: -1

    signal sectionSelected(string section)
    signal playlistSelected(int playlistId, string playlistName)
    signal createPlaylistRequested()
    signal importM3uRequested()

    implicitWidth: DesignTokens.sidebarWidth
    color: DesignTokens.sidebarBg

    component NavItem: Rectangle {
        id: itemRoot
        property string iconName: "music"
        property string label: ""
        property bool selected: false
        signal activated()

        Layout.fillWidth: true
        implicitHeight: 34
        radius: DesignTokens.radiusMd
        color: selected ? DesignTokens.surfaceSelected
                        : (ma.containsMouse ? DesignTokens.surfaceHover : "transparent")

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 11
            anchors.rightMargin: 10
            spacing: 10

            IconGlyph {
                name: itemRoot.iconName
                size: 16
                color: itemRoot.selected ? DesignTokens.accent : DesignTokens.textSecondary
            }

            Text {
                Layout.fillWidth: true
                text: itemRoot.label
                elide: Text.ElideRight
                color: itemRoot.selected ? DesignTokens.textPrimary : DesignTokens.textSecondary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 13
                font.weight: itemRoot.selected ? Font.DemiBold : Font.Normal
            }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: itemRoot.activated()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4

        Text {
            text: qsTr("LIBRARY")
            color: DesignTokens.textTertiary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.9
            Layout.leftMargin: 10
            Layout.topMargin: 4
            Layout.bottomMargin: 2
        }

        NavItem {
            iconName: "music"
            label: qsTr("All Tracks")
            selected: root.activeSection === "tracks"
            onActivated: root.sectionSelected("tracks")
        }
        NavItem {
            iconName: "album"
            label: qsTr("Albums")
            selected: root.activeSection === "albums"
            onActivated: root.sectionSelected("albums")
        }
        NavItem {
            iconName: "artist"
            label: qsTr("Artists")
            selected: root.activeSection === "artists"
            onActivated: root.sectionSelected("artists")
        }
        NavItem {
            iconName: "folder"
            label: qsTr("Folders")
            selected: root.activeSection === "folders"
            onActivated: root.sectionSelected("folders")
        }

        Text {
            text: qsTr("COLLECTIONS")
            color: DesignTokens.textTertiary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.9
            Layout.leftMargin: 10
            Layout.topMargin: 14
            Layout.bottomMargin: 2
        }

        NavItem {
            iconName: "heart"
            label: qsTr("Favorites")
            selected: root.activeSection === "favorites"
            onActivated: root.sectionSelected("favorites")
        }
        NavItem {
            iconName: "clock"
            label: qsTr("Recently Played")
            selected: root.activeSection === "recent"
            onActivated: root.sectionSelected("recent")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 4
            Layout.topMargin: 14
            Layout.bottomMargin: 2

            Text {
                Layout.fillWidth: true
                text: qsTr("PLAYLISTS")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 0.9
            }

            Rectangle {
                implicitWidth: 22; implicitHeight: 22; radius: 6
                color: impMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "import"; size: 12; color: DesignTokens.textTertiary }
                MouseArea {
                    id: impMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.importM3uRequested()
                }
            }

            Rectangle {
                implicitWidth: 22; implicitHeight: 22; radius: 6
                color: addPlMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "plus"; size: 13; color: DesignTokens.textTertiary }
                MouseArea {
                    id: addPlMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.createPlaylistRequested()
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: root.customPlaylists
            delegate: Rectangle {
                id: plDel
                required property var modelData
                width: ListView.view ? ListView.view.width : 200
                height: 32
                radius: DesignTokens.radiusMd
                readonly property bool isSel: root.activeSection === "playlist" && root.activePlaylistId === modelData.id
                color: isSel ? DesignTokens.surfaceSelected
                             : (plMa.containsMouse ? DesignTokens.surfaceHover : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 10
                    spacing: 9

                    IconGlyph {
                        name: "playlist"
                        size: 14
                        color: plDel.isSel ? DesignTokens.accent : DesignTokens.textTertiary
                    }
                    Text {
                        Layout.fillWidth: true
                        text: plDel.modelData.name
                        elide: Text.ElideRight
                        color: plDel.isSel ? DesignTokens.textPrimary : DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                }
                MouseArea {
                    id: plMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.playlistSelected(plDel.modelData.id, plDel.modelData.name)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: DesignTokens.borderSubtle
            Layout.bottomMargin: 4
        }

        NavItem {
            iconName: "settings"
            label: qsTr("Settings")
            selected: root.activeSection === "settings"
            onActivated: root.sectionSelected("settings")
        }
    }

    // Right vertical hairline
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: DesignTokens.borderSubtle
    }
}
