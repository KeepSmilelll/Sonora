import QtQuick
import QtQuick.Layouts
import Virela.UI

// Floating collapsible spatial navigation rail (60px icon rail <-> 212px expanded playlist view)
Item {
    id: root

    property bool expanded: false
    property string activeSection: "tracks"
    property var customPlaylists: []
    property int activePlaylistId: -1

    property Item hoveredNavItem: null
    property string hoveredTooltip: ""

    signal sectionSelected(string section)
    signal playlistSelected(int playlistId, string playlistName)
    signal createPlaylistRequested()
    signal importM3uRequested()
    signal toggleExpandedRequested()

    implicitWidth: expanded ? DesignTokens.railExpandedWidth : DesignTokens.railCollapsedWidth
    clip: false

    Behavior on implicitWidth {
        NumberAnimation { duration: 280; easing.type: Easing.OutQuint }
    }

    readonly property Item activeNavItem: {
        switch (root.activeSection) {
        case "tracks":    return navTracks
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
        property bool actionOnly: false
        signal activated()

        Layout.fillWidth: true
        implicitHeight: 36
        radius: DesignTokens.radiusMd
        color: !selected && ma.containsMouse ? DesignTokens.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: 130 } }

        IconGlyph {
            id: glyph
            x: 13
            anchors.verticalCenter: parent.verticalCenter
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
            anchors.left: glyph.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: itemRoot.label
            elide: Text.ElideRight
            opacity: root.expanded ? 1.0 : 0.0
            visible: opacity > 0.01
            color: itemRoot.selected
                ? DesignTokens.accentLight
                : (ma.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
            font.family: DesignTokens.fontFamily
            font.pixelSize: 13
            font.weight: itemRoot.selected ? Font.DemiBold : Font.Medium
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on color { ColorAnimation { duration: 140 } }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: {
                if (!root.expanded) {
                    root.hoveredNavItem = itemRoot
                    root.hoveredTooltip = itemRoot.label
                }
            }
            onExited: {
                if (root.hoveredNavItem === itemRoot) {
                    root.hoveredNavItem = null
                }
            }
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

    onExpandedChanged: {
        if (expanded) hoveredNavItem = null
    }

    GlassPanel {
        id: railBody
        anchors.fill: parent
        radius: DesignTokens.radiusLg
        panelColor: DesignTokens.railSurfaceBg
        borderColor: DesignTokens.hairlineBorder
        shadowEnabled: false

        // Spring-animated sliding selection indicator pill
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
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.30 : 0.22)

                Behavior on y {
                    SpringAnimation {
                        spring: 4.8
                        damping: 0.36
                        epsilon: 0.25
                    }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
                }
            }
        }

        ColumnLayout {
            id: navColumn
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 3
            z: 2

            Text {
                text: qsTr("LIBRARY")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 1.1
                Layout.leftMargin: 11
                Layout.topMargin: 2
                Layout.bottomMargin: 3
                visible: root.expanded
                opacity: root.expanded ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            NavItem {
                id: navTracks
                iconName: "music"
                label: qsTr("All Tracks")
                selected: root.activeSection === "tracks"
                onActivated: root.sectionSelected("tracks")
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

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 5
                Layout.bottomMargin: 5
                implicitHeight: 1
                color: DesignTokens.hairlineBorder
                visible: !root.expanded
            }

            Text {
                text: qsTr("COLLECTIONS")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 1.1
                Layout.leftMargin: 11
                Layout.topMargin: 10
                Layout.bottomMargin: 3
                visible: root.expanded
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

            // Collapsed playlist shortcut (expands rail to access custom playlists)
            NavItem {
                visible: !root.expanded
                iconName: "playlist"
                label: qsTr("Playlists")
                selected: root.activeSection === "playlist"
                actionOnly: true
                onActivated: root.toggleExpandedRequested()
            }

            RowLayout {
                visible: root.expanded
                Layout.fillWidth: true
                Layout.leftMargin: 11
                Layout.rightMargin: 2
                Layout.topMargin: 10
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
                visible: root.expanded
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                boundsBehavior: Flickable.StopAtBounds
                model: root.customPlaylists

                delegate: Rectangle {
                    id: plDel
                    required property var modelData
                    width: ListView.view ? ListView.view.width : 190
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
                        anchors.leftMargin: 12
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

            Item {
                visible: !root.expanded
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                implicitHeight: 1
                color: DesignTokens.hairlineBorder
                Layout.bottomMargin: 3
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

    // Floating hover tooltip when rail is collapsed
    GlassPanel {
        id: hoverTooltipPill
        readonly property bool shown: !root.expanded && root.hoveredNavItem !== null && root.hoveredTooltip.length > 0
        visible: opacity > 0.01
        opacity: shown ? 1.0 : 0.0
        z: 90
        elevated: true
        shadowEnabled: true
        radius: DesignTokens.radiusSm
        x: root.width + 10
        y: root.hoveredNavItem
            ? Math.round(root.hoveredNavItem.mapToItem(root, 0, 0).y + (root.hoveredNavItem.height - height) * 0.5)
            : 0
        width: tooltipLabel.implicitWidth + 20
        height: 28

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Text {
            id: tooltipLabel
            anchors.centerIn: parent
            text: root.hoveredTooltip
            color: DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }
}
