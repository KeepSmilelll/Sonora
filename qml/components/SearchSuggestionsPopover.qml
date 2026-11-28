import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

// Unified command-search dropdown floating below TitleBar's search capsule
Item {
    id: root

    property bool open: false
    property string query: ""
    property bool onlineConfigured: false
    property string playlistLinkId: ""
    property int bottomClearance: DesignTokens.dockClearance
    property var localResults: ({ totalTracks: 0, tracks: [], albums: [], artists: [] })
    property var anchorRect: ({ x: 200, y: 7, width: 460, height: 32 })

    signal openPlaylistLinkRequested(string rawInput)
    signal searchOnlineRequested(string query)
    signal openOnlineSettingsRequested()
    signal playLocalTrackRequested(int trackId)
    signal openLocalAlbumRequested(string album, string albumArtist, string coverHash, int year)
    signal openLocalArtistRequested(string artist)
    signal filterLocalLibraryRequested(string query)

    readonly property string trimmedQuery: query.trim()
    readonly property var localTracks: (localResults && localResults.tracks) ? localResults.tracks : []
    readonly property var localAlbums: (localResults && localResults.albums) ? localResults.albums : []
    readonly property var localArtists: (localResults && localResults.artists) ? localResults.artists : []
    readonly property int localTotalTracks: (localResults && localResults.totalTracks) ? localResults.totalTracks : 0
    readonly property bool hasLocalMatches: localTracks.length > 0 || localAlbums.length > 0 || localArtists.length > 0
    readonly property bool shown: open && trimmedQuery.length > 0

    function isPointInside(wx, wy) {
        if (!panel.visible || panel.opacity < 0.05) return false
        const p = panel.mapToItem(null, 0, 0)
        return wx >= p.x && wx <= p.x + panel.width
            && wy >= p.y && wy <= p.y + panel.height
    }

    anchors.fill: parent
    visible: panel.opacity > 0.01

    GlassPanel {
        id: panel
        readonly property real targetW: Math.max(360, Math.min(parent.width - 32,
            Math.max(root.anchorRect.width + 48, Math.min(680, root.anchorRect.width + 64))))
        x: Math.max(16, Math.min(parent.width - width - 16, Math.round(root.anchorRect.x + (root.anchorRect.width - width) * 0.5)))
        y: Math.round(root.anchorRect.y + root.anchorRect.height + 8)
        width: targetW
        height: Math.min(Math.max(220, parent.height - y - root.bottomClearance - 12), 460, scrollView.contentHeight + 16)
        radius: DesignTokens.radiusLg
        elevated: true
        shadowEnabled: true
        opacity: root.shown ? 1.0 : 0.0
        scale: root.shown ? 1.0 : 0.96
        transformOrigin: Item.Top
        visible: opacity > 0.01

        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        // Eat background clicks inside popover
        MouseArea { anchors.fill: parent }

        Flickable {
            id: scrollView
            anchors.fill: parent
            anchors.margins: 8
            contentWidth: width
            contentHeight: contentCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: contentCol
                width: scrollView.width
                spacing: 4

                // 1. Detected Netease Playlist Link / ID Banner
                Rectangle {
                    visible: root.playlistLinkId.length > 0
                    Layout.fillWidth: true
                    implicitHeight: 42
                    radius: DesignTokens.radiusMd
                    color: plHitMa.containsMouse ? DesignTokens.surfaceSelected : DesignTokens.accentSoft
                    border.width: 1
                    border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.40)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        IconGlyph { name: "link"; size: 15; color: DesignTokens.accent }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Open Netease Playlist #%1").arg(root.playlistLinkId)
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        FormatBadge { compact: true; text: "Enter" }
                    }

                    MouseArea {
                        id: plHitMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openPlaylistLinkRequested(root.trimmedQuery)
                    }
                }

                // 2. Online Search Action Row
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: DesignTokens.radiusMd
                    color: onlMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                    Behavior on color { ColorAnimation { duration: 110 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Rectangle {
                            implicitWidth: 26
                            implicitHeight: 26
                            radius: 8
                            color: DesignTokens.accentSoft
                            IconGlyph { anchors.centerIn: parent; name: "cloud"; size: 13; color: DesignTokens.accent }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.onlineConfigured
                                ? qsTr("Search online for \"%1\"").arg(root.trimmedQuery)
                                : qsTr("Configure online sources to search streaming music")
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }

                        FormatBadge {
                            compact: true
                            text: root.onlineConfigured ? "Enter" : qsTr("Setup")
                        }
                    }

                    MouseArea {
                        id: onlMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.onlineConfigured) root.searchOnlineRequested(root.trimmedQuery)
                            else root.openOnlineSettingsRequested()
                        }
                    }
                }

                // Divider before local matches
                Rectangle {
                    visible: root.hasLocalMatches
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    implicitHeight: 1
                    color: DesignTokens.hairlineBorder
                }

                // 3. Local Library Section Header
                RowLayout {
                    visible: root.hasLocalMatches
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 2
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Local Library")
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Text {
                        visible: root.localTotalTracks > 0
                        text: qsTr("%1 track(s)").arg(root.localTotalTracks)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                    }
                }

                // Local Matching Tracks
                Repeater {
                    model: root.localTracks
                    delegate: Rectangle {
                        id: trRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: DesignTokens.radiusSm
                        color: trMa.containsMouse ? DesignTokens.surfaceHover : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 7
                                color: DesignTokens.surfaceInput
                                clip: true

                                Image {
                                    id: trImg
                                    anchors.fill: parent
                                    source: trRow.modelData.coverHash
                                        ? ("image://cover/" + trRow.modelData.coverHash + "?size=64")
                                        : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                    visible: status === Image.Ready
                                }
                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: trImg.status !== Image.Ready
                                    name: "music"
                                    size: 13
                                    color: DesignTokens.textTertiary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: trRow.modelData.title || ""
                                    elide: Text.ElideRight
                                    color: DesignTokens.textPrimary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (trRow.modelData.artist || qsTr("Unknown Artist"))
                                        + (trRow.modelData.album ? (" · " + trRow.modelData.album) : "")
                                    elide: Text.ElideRight
                                    color: DesignTokens.textTertiary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 11
                                }
                            }

                            FormatBadge {
                                compact: true
                                visible: Boolean(trRow.modelData.formatBadge)
                                text: trRow.modelData.formatBadge || ""
                            }
                        }

                        MouseArea {
                            id: trMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.playLocalTrackRequested(trRow.modelData.trackId)
                        }
                    }
                }

                // Local Matching Albums
                Repeater {
                    model: root.localAlbums
                    delegate: Rectangle {
                        id: alRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: DesignTokens.radiusSm
                        color: alMa.containsMouse ? DesignTokens.surfaceHover : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 6
                                color: DesignTokens.surfaceInput
                                clip: true
                                Image {
                                    id: alImg
                                    anchors.fill: parent
                                    source: alRow.modelData.coverHash
                                        ? ("image://cover/" + alRow.modelData.coverHash + "?size=64")
                                        : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                    visible: status === Image.Ready
                                }
                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: alImg.status !== Image.Ready
                                    name: "album"
                                    size: 13
                                    color: DesignTokens.textTertiary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: (alRow.modelData.album || "") + " — " + (alRow.modelData.albumArtist || "")
                                elide: Text.ElideRight
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                            }

                            FormatBadge { compact: true; text: qsTr("Album") }
                        }

                        MouseArea {
                            id: alMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openLocalAlbumRequested(
                                alRow.modelData.album || "",
                                alRow.modelData.albumArtist || "",
                                alRow.modelData.coverHash || "",
                                alRow.modelData.year || 0)
                        }
                    }
                }

                // Local Matching Artists
                Repeater {
                    model: root.localArtists
                    delegate: Rectangle {
                        id: arRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: DesignTokens.radiusSm
                        color: arMa.containsMouse ? DesignTokens.surfaceHover : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 14
                                color: DesignTokens.accentSoft
                                IconGlyph { anchors.centerIn: parent; name: "artist"; size: 13; color: DesignTokens.accent }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: arRow.modelData.artist || ""
                                elide: Text.ElideRight
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                            }

                            FormatBadge { compact: true; text: qsTr("Artist") }
                        }

                        MouseArea {
                            id: arMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openLocalArtistRequested(arRow.modelData.artist || "")
                        }
                    }
                }

                // Filter All Local Library Row
                Rectangle {
                    visible: root.localTotalTracks > 0
                    Layout.fillWidth: true
                    implicitHeight: 36
                    radius: DesignTokens.radiusSm
                    color: fltMa.containsMouse ? DesignTokens.surfaceSelected : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        IconGlyph { name: "search"; size: 13; color: DesignTokens.accent }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Filter all %1 local track(s) for \"%2\"").arg(root.localTotalTracks).arg(root.trimmedQuery)
                            elide: Text.ElideRight
                            color: DesignTokens.accentLight
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                        IconGlyph { name: "chevron_right"; size: 12; color: DesignTokens.accent }
                    }

                    MouseArea {
                        id: fltMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.filterLocalLibraryRequested(root.trimmedQuery)
                    }
                }
            }
        }
    }
}
