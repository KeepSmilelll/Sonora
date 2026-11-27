import QtQuick
import QtQuick.Layouts
import Virela.UI

GlassPanel {
    id: root

    property bool compact: false
    property string activeSection: "tracks"
    property var customPlaylists: []
    property int activePlaylistId: -1

    signal sectionSelected(string section)
    signal playlistSelected(int playlistId, string playlistName)
    signal createPlaylistRequested()
    signal importM3uRequested()

    implicitWidth: compact ? DesignTokens.compactSidebarWidth : DesignTokens.sidebarWidth
    radius: DesignTokens.radiusLg
    panelColor: DesignTokens.isDark ? "#12141d" : DesignTokens.sidebarGlass
    borderColor: DesignTokens.borderSubtle
    shadowEnabled: false

    readonly property Item activeNavItem: {
        switch (root.activeSection) {
        case "tracks":    return navTracks
        case "albums":    return navAlbums
        case "artists":   return navArtists
        case "folders":   return navFolders
        case "favorites": return navFavorites
        case "recent":    return navRecent
        case "settings":  return navSettings
        default:          return null
        }
    }

    component NavItem: Rectangle {
        id: itemRoot
        property string iconName: "music"
        property string label: ""
        property bool selected: false
        signal activated()

        Layout.fillWidth: true
        implicitHeight: 36
        radius: DesignTokens.radiusSm
        color: !selected && ma.containsMouse ? DesignTokens.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: 130 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 10
            spacing: 11

            IconGlyph {
                name: itemRoot.iconName
                size: 16
                color: itemRoot.selected
                    ? DesignTokens.accent
                    : (ma.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                scale: ma.pressed ? 0.90 : (itemRoot.selected ? 1.06 : 1.0)
                Behavior on scale {
                    NumberAnimation { duration: 180; easing.type: Easing.OutBack }
                }
            }

            Text {
                Layout.fillWidth: true
                text: itemRoot.label
                elide: Text.ElideRight
                color: itemRoot.selected
                    ? DesignTokens.accentLight
                    : (ma.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                font.family: DesignTokens.fontFamily
                font.pixelSize: 13
                font.weight: itemRoot.selected ? Font.DemiBold : Font.Medium
                Behavior on color { ColorAnimation { duration: 140 } }
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

    component SmallToolBtn: Rectangle {
        id: toolBtn
        property string iconName: "plus"
        property real iconSize: 12
        signal clicked()

        implicitWidth: 24
        implicitHeight: 24
        radius: 7
        color: toolMa.pressed
            ? DesignTokens.surfacePressed
            : (toolMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
        Behavior on color { ColorAnimation { duration: 120 } }

        scale: toolMa.pressed ? 0.90 : (toolMa.containsMouse ? 1.08 : 1.0)
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }

        IconGlyph {
            anchors.centerIn: parent
            name: toolBtn.iconName
            size: toolBtn.iconSize
            color: toolMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textTertiary
        }

        MouseArea {
            id: toolMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: toolBtn.clicked()
        }
    }

    // Shared sliding selection indicator pill behind fixed navigation items
    Item {
        id: indicatorLayer
        anchors.fill: navColumn
        z: 1

        Rectangle {
            id: selectionPill
            x: 0
            width: indicatorLayer.width
            y: root.activeNavItem ? root.activeNavItem.y : 0
            height: root.activeNavItem ? root.activeNavItem.height : 36
            opacity: root.activeNavItem !== null ? 1.0 : 0.0
            radius: DesignTokens.radiusMd
            color: DesignTokens.surfaceSelected
            border.width: 1
            border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.30 : 0.20)

            Behavior on y {
                NumberAnimation { duration: 250; easing.type: Easing.OutQuint }
            }
            Behavior on opacity {
                NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
            }

        }
    }

    ColumnLayout {
        id: navColumn
        anchors.fill: parent
        anchors.margins: root.compact ? 10 : 16
        spacing: 2
        z: 2

        Text {
            text: qsTr("LIBRARY")
            color: DesignTokens.textTertiary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 1.1
            Layout.leftMargin: 12
            Layout.topMargin: 4
            Layout.bottomMargin: 3
        }

        NavItem {
            id: navTracks
            iconName: "music"
            label: qsTr("All Tracks")
            selected: root.activeSection === "tracks"
            onActivated: root.sectionSelected("tracks")
        }
        NavItem {
            id: navAlbums
            iconName: "album"
            label: qsTr("Albums")
            selected: root.activeSection === "albums"
            onActivated: root.sectionSelected("albums")
        }
        NavItem {
            id: navArtists
            iconName: "artist"
            label: qsTr("Artists")
            selected: root.activeSection === "artists"
            onActivated: root.sectionSelected("artists")
        }
        NavItem {
            id: navFolders
            iconName: "folder"
            label: qsTr("Folders")
            selected: root.activeSection === "folders"
            onActivated: root.sectionSelected("folders")
        }

        Text {
            text: qsTr("COLLECTIONS")
            color: DesignTokens.textTertiary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 1.1
            Layout.leftMargin: 12
            Layout.topMargin: root.compact ? 6 : 12
            Layout.bottomMargin: 3
        }

        NavItem {
            id: navFavorites
            iconName: root.activeSection === "favorites" ? "heart_filled" : "heart"
            label: qsTr("Favorites")
            selected: root.activeSection === "favorites"
            onActivated: root.sectionSelected("favorites")
        }
        NavItem {
            id: navRecent
            iconName: "clock"
            label: qsTr("Recently Played")
            selected: root.activeSection === "recent"
            onActivated: root.sectionSelected("recent")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 2
            Layout.topMargin: root.compact ? 6 : 12
            Layout.bottomMargin: 3
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: qsTr("PLAYLISTS")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 1.1
            }

            SmallToolBtn {
                iconName: "import"
                iconSize: 12
                onClicked: root.importM3uRequested()
            }

            SmallToolBtn {
                iconName: "plus"
                iconSize: 13
                onClicked: root.createPlaylistRequested()
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            boundsBehavior: Flickable.StopAtBounds
            model: root.customPlaylists

            delegate: Rectangle {
                id: plDel
                required property var modelData
                width: ListView.view ? ListView.view.width : 200
                height: 34
                radius: DesignTokens.radiusMd
                readonly property bool isSel: root.activeSection === "playlist" && root.activePlaylistId === modelData.id

                color: isSel
                    ? DesignTokens.surfaceSelected
                    : (plMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                border.width: isSel ? 1 : 0
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.30 : 0.20)
                Behavior on color { ColorAnimation { duration: 130 } }

                Rectangle {
                    visible: plDel.isSel
                    width: 3
                    height: 14
                    radius: 1.5
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: DesignTokens.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 10
                    spacing: 10

                    IconGlyph {
                        name: "playlist"
                        size: 14
                        color: plDel.isSel ? DesignTokens.accent : DesignTokens.textTertiary
                    }
                    Text {
                        Layout.fillWidth: true
                        text: plDel.modelData.name
                        elide: Text.ElideRight
                        color: plDel.isSel
                            ? DesignTokens.textPrimary
                            : (plMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: plDel.isSel ? Font.DemiBold : Font.Normal
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
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            implicitHeight: 1
            color: DesignTokens.borderSubtle
            Layout.bottomMargin: 4
        }

        NavItem {
            id: navSettings
            iconName: "settings"
            label: qsTr("Settings")
            selected: root.activeSection === "settings"
            onActivated: root.sectionSelected("settings")
        }
    }
}
