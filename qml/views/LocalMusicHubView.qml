import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var lib
    property var heroLayer: null
    property string activeTab: "tracks" // "tracks" | "albums" | "artists" | "folders"

    signal tabChanged(string tab)
    signal addFolderRequested()
    signal openFilesRequested()
    signal inspectTrack(var trackMap)

    readonly property bool compact: width < 680

    function openAlbumDetail(albumName, albumArtist, coverHash, year) {
        if (root.activeTab !== "albums") {
            root.tabChanged("albums")
        }
        Qt.callLater(() => {
            if (albumsLoader.item) {
                albumsLoader.item.openAlbumByInfo(albumName, albumArtist, coverHash, year)
            }
        })
    }

    function selectArtist(artistName) {
        if (root.activeTab !== "artists") {
            root.tabChanged("artists")
        }
        Qt.callLater(() => {
            if (artistsLoader.item) {
                artistsLoader.item.selectArtist(artistName)
            }
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.width < 680 ? 16 : (root.width >= 1560 ? 26 : 22)
        spacing: 12

        // Top Header Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Local Music")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 24
                font.weight: Font.Bold
                font.letterSpacing: -0.3
            }

            Rectangle {
                implicitWidth: Math.min(240, statusLbl.implicitWidth + 18)
                implicitHeight: 24
                radius: 12
                color: root.lib.scanning ? DesignTokens.accentSoft : DesignTokens.surfaceInput
                border.width: 1
                border.color: root.lib.scanning
                    ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.30)
                    : DesignTokens.hairlineBorder

                Text {
                    id: statusLbl
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, parent.width - 14)
                    elide: Text.ElideRight
                    text: root.lib.scanning
                        ? qsTr("Scanning... (%1 / %2)").arg(root.lib.scanProgress).arg(root.lib.scanTotal)
                        : qsTr("%1 track(s)").arg(root.lib.totalTrackCount)
                    color: root.lib.scanning ? DesignTokens.accent : DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: root.lib.scanning ? Font.DemiBold : Font.Medium
                    font.features: ({ "tnum": 1 })
                }
            }

            // Active local filter chip (clearable)
            Rectangle {
                visible: root.lib.searchQuery.length > 0
                implicitWidth: Math.min(200, filterLbl.implicitWidth + 32)
                implicitHeight: 24
                radius: 12
                color: DesignTokens.accentSoft
                border.width: 1
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.34)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 7
                    spacing: 5
                    Text {
                        id: filterLbl
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: qsTr("Filter: %1").arg(root.lib.searchQuery)
                        color: DesignTokens.accentLight
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    IconGlyph {
                        name: "close"
                        size: 10
                        color: DesignTokens.accentLight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.searchQuery = ""
                }
            }

            Item { Layout.fillWidth: true }

            VirelaButton {
                visible: !root.compact
                compact: true
                iconName: "refresh"
                enabled: !root.lib.scanning
                text: root.lib.scanning ? qsTr("Scanning...") : qsTr("Rescan")
                onClicked: root.lib.rescanLibrary()
            }

            VirelaButton {
                compact: true
                iconName: "folder"
                text: qsTr("Add Folder")
                onClicked: root.addFolderRequested()
            }

            VirelaButton {
                visible: !root.compact
                compact: true
                iconName: "plus"
                text: qsTr("Open Files")
                onClicked: root.openFilesRequested()
            }
        }

        // Segmented Sub-View Tabs (All Tracks / Albums / Artists / Folders)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { key: "tracks",  label: qsTr("All Tracks"), count: root.lib.tracksModel.count },
                    { key: "albums",  label: qsTr("Albums"),     count: root.lib.albumsModel.count },
                    { key: "artists", label: qsTr("Artists"),    count: root.lib.artistsModel.count },
                    { key: "folders", label: qsTr("Folders"),    count: -1 }
                ]
                delegate: Rectangle {
                    id: subTab
                    required property var modelData
                    readonly property bool active: root.activeTab === modelData.key
                    implicitWidth: tabRow.implicitWidth + 24
                    implicitHeight: 30
                    radius: 15
                    color: active ? DesignTokens.surfaceSelected : (stMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceInput)
                    border.width: 1
                    border.color: active
                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.42)
                        : DesignTokens.hairlineBorder
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Row {
                        id: tabRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: subTab.modelData.label
                            color: subTab.active ? DesignTokens.accentLight : DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.weight: subTab.active ? Font.DemiBold : Font.Medium
                        }
                        Text {
                            visible: subTab.modelData.count > 0
                            text: String(subTab.modelData.count)
                            color: subTab.active ? DesignTokens.accent : DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            font.features: ({ "tnum": 1 })
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: stMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (!subTab.active) root.tabChanged(subTab.modelData.key)
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Embedded Sub-View Viewport
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                anchors.fill: parent
                active: root.activeTab === "tracks"
                visible: active
                sourceComponent: TracksView {
                    embedded: true
                    controller: root.controller
                    lib: root.lib
                    sectionKey: "tracks"
                    sectionTitle: qsTr("All Tracks")
                    onAddFolderRequested: root.addFolderRequested()
                    onOpenFilesRequested: root.openFilesRequested()
                    onInspectTrack: (map) => root.inspectTrack(map)
                }
            }

            Loader {
                id: albumsLoader
                anchors.fill: parent
                active: root.activeTab === "albums"
                visible: active
                sourceComponent: AlbumsView {
                    embedded: true
                    controller: root.controller
                    lib: root.lib
                    heroLayer: root.heroLayer
                }
            }

            Loader {
                id: artistsLoader
                anchors.fill: parent
                active: root.activeTab === "artists"
                visible: active
                sourceComponent: ArtistsView {
                    embedded: true
                    controller: root.controller
                    lib: root.lib
                }
            }

            Loader {
                anchors.fill: parent
                active: root.activeTab === "folders"
                visible: active
                sourceComponent: FoldersView {
                    embedded: true
                    controller: root.controller
                    lib: root.lib
                    onAddFolderRequested: root.addFolderRequested()
                }
            }
        }
    }
}
