import QtQuick
import QtQuick.Layouts
import Virela.UI

// Floating navigation rail with icon-only and expanded layouts.
Item {
    id: root

    property bool expanded: false
    property int expandedWidth: DesignTokens.railExpandedWidth
    property bool compactDensity: false
    property string activeSection: "online"
    property var customPlaylists: []
    property var pinnedPlaylists: []
    property int activePlaylistId: -1
    property string activePinnedRemoteId: ""
    property int activeDownloadCount: 0

    property Item hoveredNavItem: null
    property string hoveredTooltip: ""
    property real hoveredTooltipY: 0

    onHoveredNavItemChanged: {
        if (hoveredNavItem) {
            hoveredTooltipY = Math.round(
                hoveredNavItem.mapToItem(root, 0, 0).y
                + (hoveredNavItem.height - hoverTooltipPill.height) * 0.5)
        }
    }

    signal sectionSelected(string section)
    signal playlistSelected(int playlistId, string playlistName)
    signal pinnedPlaylistSelected(string source, string remoteId, string name, string creator, string coverUrl)
    signal unpinOnlinePlaylistRequested(string source, string remoteId)
    signal createPlaylistRequested()
    signal importM3uRequested()
    signal toggleExpandedRequested()

    implicitWidth: expanded ? root.expandedWidth : DesignTokens.railCollapsedWidth
    clip: false

    Behavior on implicitWidth {
        NumberAnimation { duration: 280; easing.type: Easing.OutQuint }
    }

    readonly property Item activeNavItem: {
        switch (root.activeSection) {
        case "online":    return navOnline
        case "favorites": return navFavorites
        case "recent":    return navRecent
        case "local":     return navLocal
        case "downloads": return navDownloads
        case "playlist":  return root.expanded ? null : navPlaylists
        case "settings":  return navSettings
        default:          return null
        }
    }

    component NavItem: Rectangle {
        id: itemRoot
        property string iconName: "music"
        property string label: ""
        property bool selected: false
        property int badgeCount: 0
        signal activated()

        Layout.fillWidth: true
        implicitHeight: root.compactDensity ? 33 : 36
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

        // Compact dot when rail is collapsed and item has active badgeCount
        Rectangle {
            visible: !root.expanded && itemRoot.badgeCount > 0
            width: 6
            height: 6
            radius: 3
            x: glyph.x + glyph.width - 2
            y: glyph.y - 2
            color: DesignTokens.accent
        }

        Text {
            anchors.left: glyph.right
            anchors.leftMargin: 12
            anchors.right: itemBadge.visible ? itemBadge.left : parent.right
            anchors.rightMargin: itemBadge.visible ? 6 : 10
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

        Rectangle {
            id: itemBadge
            visible: root.expanded && itemRoot.badgeCount > 0
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: Math.max(18, badgeLbl.implicitWidth + 10)
            implicitHeight: 18
            radius: 9
            color: DesignTokens.accentSoft
            border.width: 1
            border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.35)

            Text {
                id: badgeLbl
                anchors.centerIn: parent
                text: String(itemRoot.badgeCount)
                color: DesignTokens.accentLight
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.features: ({ "tnum": 1 })
            }
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
                x: root.activeNavItem ? root.activeNavItem.x : 0
                width: root.activeNavItem ? root.activeNavItem.width : indicatorLayer.width
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
            anchors.topMargin: root.compactDensity ? 9 : 12
            anchors.bottomMargin: root.compactDensity ? 9 : 12
            spacing: root.compactDensity ? 2 : 3
            z: 2

            // Group 1: DISCOVER
            Text {
                visible: root.expanded
                Layout.leftMargin: 11
                Layout.topMargin: 1
                Layout.bottomMargin: 2
                text: qsTr("DISCOVER")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.6
            }

            NavItem {
                id: navOnline
                iconName: "cloud"
                label: qsTr("Discover")
                selected: root.activeSection === "online"
                onActivated: root.sectionSelected("online")
            }

            // Group divider (collapsed hairline or expanded LIBRARY label)
            Rectangle {
                visible: !root.expanded
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                implicitWidth: 24
                implicitHeight: 1
                color: DesignTokens.hairlineBorder
            }

            Text {
                visible: root.expanded
                Layout.leftMargin: 11
                Layout.topMargin: 8
                Layout.bottomMargin: 2
                text: qsTr("LIBRARY")
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.6
            }

            // Group 2: LIBRARY (unified across online & local)
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
            NavItem {
                id: navLocal
                iconName: "music"
                label: qsTr("Local Music")
                selected: root.activeSection === "local"
                onActivated: root.sectionSelected("local")
            }
            NavItem {
                id: navDownloads
                iconName: "download"
                label: qsTr("Downloads")
                selected: root.activeSection === "downloads"
                badgeCount: root.activeDownloadCount
                onActivated: root.sectionSelected("downloads")
            }

            // Collapsed playlist shortcut (expands rail to access custom playlists)
            NavItem {
                id: navPlaylists
                visible: !root.expanded
                iconName: "playlist"
                label: qsTr("Playlists")
                selected: root.activeSection === "playlist"
                onActivated: root.toggleExpandedRequested()
            }

            // Group 3: PLAYLISTS (expanded)
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
                    font.letterSpacing: 0.6
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
                model: visible ? root.customPlaylists : null

                delegate: Rectangle {
                    id: plDel
                    required property var modelData
                    width: ListView.view ? ListView.view.width : 190
                    height: root.compactDensity ? 31 : 34
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

                footer: Column {
                    width: parent ? parent.width : 190
                    spacing: 3
                    visible: root.pinnedPlaylists && root.pinnedPlaylists.length > 0

                    Rectangle {
                        width: parent.width - 16
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 1
                        color: DesignTokens.hairlineBorder
                    }

                    Text {
                        leftPadding: 11
                        topPadding: 6
                        bottomPadding: 2
                        text: qsTr("PINNED ONLINE")
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 0.6
                    }

                    Repeater {
                        model: root.pinnedPlaylists
                        delegate: Rectangle {
                            id: pinDel
                            required property var modelData
                            readonly property bool isSel: root.activeSection === "online"
                                && root.activePinnedRemoteId === (modelData.remoteId || "")
                            width: ListView.view ? ListView.view.width : 190
                            height: root.compactDensity ? 31 : 34
                            radius: DesignTokens.radiusMd
                            color: isSel
                                ? DesignTokens.surfaceSelected
                                : ((pinMa.containsMouse || unpinMa.containsMouse) ? DesignTokens.surfaceHover : "transparent")
                            border.width: isSel ? 1 : 0
                            border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.30 : 0.20)
                            Behavior on color { ColorAnimation { duration: 130 } }

                            MouseArea {
                                id: pinMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.pinnedPlaylistSelected(
                                    pinDel.modelData.source || "netease",
                                    pinDel.modelData.remoteId || "",
                                    pinDel.modelData.name || "",
                                    pinDel.modelData.creator || "",
                                    pinDel.modelData.coverUrl || "")
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 6
                                spacing: 8

                                IconGlyph {
                                    name: "cloud"
                                    size: 13
                                    color: pinDel.isSel ? DesignTokens.accent : DesignTokens.accentLight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: pinDel.modelData.name || ""
                                    elide: Text.ElideRight
                                    color: pinDel.isSel
                                        ? DesignTokens.textPrimary
                                        : (pinMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 13
                                    font.weight: pinDel.isSel ? Font.DemiBold : Font.Normal
                                }

                                Rectangle {
                                    visible: pinMa.containsMouse || unpinMa.containsMouse
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    radius: 6
                                    color: unpinMa.containsMouse ? DesignTokens.surfacePressed : "transparent"

                                    IconGlyph {
                                        anchors.centerIn: parent
                                        name: "close"
                                        size: 11
                                        color: DesignTokens.textTertiary
                                    }

                                    MouseArea {
                                        id: unpinMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.unpinOnlinePlaylistRequested(
                                            pinDel.modelData.source || "netease",
                                            pinDel.modelData.remoteId || "")
                                    }
                                }
                            }
                        }
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
        property string displayedTooltip: ""
        readonly property bool shown: !root.expanded && root.hoveredNavItem !== null && root.hoveredTooltip.length > 0
        visible: opacity > 0.01
        opacity: shown ? 1.0 : 0.0
        scale: shown ? 1.0 : 0.92
        transformOrigin: Item.Left
        z: 90
        elevated: true
        shadowEnabled: true
        radius: DesignTokens.radiusSm
        x: root.width + 10
        y: root.hoveredTooltipY
        width: tooltipLabel.implicitWidth + 20
        height: 28

        transform: Translate {
            x: hoverTooltipPill.shown ? 0 : -6
            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutQuint } }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: hoverTooltipPill.shown ? 160 : 120
                easing.type: hoverTooltipPill.shown ? Easing.OutCubic : Easing.InCubic
            }
        }
        Behavior on scale { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on y {
            enabled: hoverTooltipPill.opacity > 0.25
            NumberAnimation { duration: 150; easing.type: Easing.OutQuint }
        }

        onShownChanged: if (shown) displayedTooltip = root.hoveredTooltip
        onOpacityChanged: if (opacity <= 0.01 && !shown) displayedTooltip = ""

        Connections {
            target: root
            function onHoveredTooltipChanged() {
                if (hoverTooltipPill.shown && root.hoveredTooltip.length > 0) {
                    hoverTooltipPill.displayedTooltip = root.hoveredTooltip
                }
            }
        }

        Text {
            id: tooltipLabel
            anchors.centerIn: parent
            text: hoverTooltipPill.displayedTooltip
            color: DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }
}
