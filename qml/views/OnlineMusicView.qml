import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var onlineCtrl
    required property var lib
    required property bool neteaseConfigured
    property int localMatchCount: 0

    signal openSettingsRequested()
    signal openLocalRequested()
    signal openRecentRequested()
    signal openLocalAlbumRequested(string albumName, string artistName, string coverHash)
    signal viewLocalMatchesRequested(string query)
    signal focusSearchRequested()
    signal inspectTrack(var trackMap)

    readonly property bool compact: width < 680
    readonly property real _dpr: Math.max(1.0, Math.min(2.0, root.Screen.devicePixelRatio || 1.0))
    readonly property int cardGridColumns: width >= 2000 ? 5 : (width >= 1560 ? 4 : (width >= 1100 ? 3 : (width >= 660 ? 2 : 1)))
    readonly property int carouselCardWidth: width >= 1560 ? 184 : (width >= 1120 ? 168 : 152)
    readonly property int searchCardBaseWidth: width >= 1900 ? 222 : (width >= 1560 ? 208 : (width >= 1120 ? 194 : 178))
    readonly property int cardCoverPx: Math.min(320, Math.round(200 * _dpr))
    readonly property bool isDiscoverHome: !root.onlineCtrl.inDetail && root.onlineCtrl.searchQuery.trim().length === 0
    readonly property bool isSearchView: !root.onlineCtrl.inDetail && root.onlineCtrl.searchQuery.trim().length > 0
    readonly property bool hasDiscoverOnlineContent: root.neteaseConfigured
        && (root.onlineCtrl.recommendedPlaylistsModel.count > 0
            || root.onlineCtrl.newSongsModel.count > 0)
    readonly property bool hasLocalDiscoverContent: root.lib.recentTracksSummary.length > 0
        || root.lib.recentAlbumsSummary.length > 0
    readonly property bool detailPlaylistPinned: {
        const list = root.lib.pinnedPlaylists
        if (!list || root.onlineCtrl.detailType !== "playlist") return false
        const rid = root.onlineCtrl.detailRemoteId
        for (let i = 0; i < list.length; ++i) {
            if (list[i].remoteId === rid) return true
        }
        return false
    }

    Component.onCompleted: {
        if (root.neteaseConfigured) {
            root.onlineCtrl.ensureDiscoverLoaded()
        }
    }

    Connections {
        target: root.onlineCtrl
        function onBackendConfigChanged() {
            if (root.neteaseConfigured) {
                root.onlineCtrl.ensureDiscoverLoaded()
            }
        }
    }

    component StyledMenuItem: MenuItem {
        id: mi
        implicitHeight: 32
        implicitWidth: 196

        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            text: mi.text
            color: mi.highlighted ? DesignTokens.accent : DesignTokens.textPrimary
            font.family: DesignTokens.fontFamily
            font.pixelSize: 12
            font.weight: mi.highlighted ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: DesignTokens.radiusSm
            color: mi.highlighted ? DesignTokens.surfaceSelected : "transparent"
        }
    }

    component InlineIconBtn: Rectangle {
        id: iBtn
        property string iconName: "plus"
        property bool activeAccent: false
        readonly property bool hovered: iMa.containsMouse
        signal clicked()

        implicitWidth: 28
        implicitHeight: 28
        radius: 8
        color: iMa.pressed ? DesignTokens.surfacePressed : (iMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
        scale: iMa.pressed ? 0.90 : (iMa.containsMouse ? 1.07 : 1.0)
        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: 110 } }

        IconGlyph {
            anchors.centerIn: parent
            name: iBtn.iconName
            size: 13
            color: iBtn.activeAccent
                ? DesignTokens.badgeTeal
                : (iMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
        }

        MouseArea {
            id: iMa
            anchors.fill: parent
            enabled: iBtn.enabled
            hoverEnabled: true
            cursorShape: iBtn.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: iBtn.clicked()
        }
    }

    component TrackListDelegate: Rectangle {
        id: tRow
        required property int index
        required property string title
        required property string artist
        required property string album
        required property int durationMs
        required property string formatBadge
        required property string coverUrl
        required property string remoteId
        required property string trackKey
        required property string downloadStatus
        property bool detailMode: false

        readonly property bool isCurrent: root.controller.trackIsOnline
            && root.controller.trackKey === tRow.trackKey
        readonly property bool isDlActive: tRow.downloadStatus === "downloading" || tRow.downloadStatus === "queued"
        readonly property bool hovered: rowMa.containsMouse || thumbMa.containsMouse
            || playNextBtn.hovered || queueBtn.hovered || dlBtn.hovered || infoBtn.hovered

        width: ListView.view ? ListView.view.width : 540
        height: 48
        radius: DesignTokens.radiusSm
        color: isCurrent
            ? DesignTokens.surfaceSelected
            : (hovered ? DesignTokens.surfaceHover : "transparent")
        border.width: isCurrent ? 1 : 0
        border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.28)
        Behavior on color { ColorAnimation { duration: 110 } }

        Rectangle {
            visible: tRow.isCurrent
            width: 3
            height: 18
            radius: 1.5
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            color: DesignTokens.accent
        }

        MouseArea {
            id: rowMa
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onDoubleClicked: {
                if (tRow.detailMode) root.onlineCtrl.playDetailAt(tRow.index)
                else root.onlineCtrl.playSongAt(tRow.index)
            }
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    const mdl = tRow.detailMode ? root.onlineCtrl.detailTracksModel : root.onlineCtrl.songsModel
                    songContextMenu.openFor(tRow.index, tRow.detailMode ? "detail" : "songs", mdl.trackMapAt(tRow.index))
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            Item {
                Layout.preferredWidth: 24
                implicitHeight: 18

                NowPlayingBars {
                    anchors.centerIn: parent
                    visible: tRow.isCurrent && tRow.ListView.view !== null
                        && tRow.y + tRow.height > tRow.ListView.view.contentY
                        && tRow.y < tRow.ListView.view.contentY + tRow.ListView.view.height
                    controller: root.controller
                    size: 13
                }

                Text {
                    anchors.centerIn: parent
                    visible: !tRow.isCurrent
                    text: String(tRow.index + 1).padStart(2, "0")
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.features: ({ "tnum": 1 })
                }
            }

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 7
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: tRow.isCurrent ? DesignTokens.accent : DesignTokens.hairlineBorder
                clip: true

                Image {
                    id: rowCover
                    anchors.fill: parent
                    source: tRow.coverUrl
                    sourceSize: Qt.size(80, 80)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: status === Image.Ready
                }

                IconGlyph {
                    anchors.centerIn: parent
                    visible: rowCover.status !== Image.Ready && !thumbMa.containsMouse
                    name: "music"
                    size: 15
                    color: DesignTokens.textTertiary
                }

                Rectangle {
                    anchors.fill: parent
                    visible: thumbMa.containsMouse
                    color: rowCover.visible ? Qt.rgba(0, 0, 0, 0.46) : "transparent"

                    IconGlyph {
                        anchors.centerIn: parent
                        name: tRow.isCurrent && root.controller.isPlaying ? "pause" : "play"
                        size: 14
                        color: rowCover.visible ? "#ffffff" : DesignTokens.accent
                    }
                }

                MouseArea {
                    id: thumbMa
                    anchors.fill: parent
                    z: 5
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (tRow.isCurrent) root.controller.togglePlayPause()
                        else if (tRow.detailMode) root.onlineCtrl.playDetailAt(tRow.index)
                        else root.onlineCtrl.playSongAt(tRow.index)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: tRow.title
                    elide: Text.ElideRight
                    color: tRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 13
                    font.weight: tRow.isCurrent ? Font.DemiBold : Font.Medium
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    IconGlyph {
                        name: tRow.downloadStatus === "done" ? "check" : "cloud"
                        size: 10
                        color: tRow.downloadStatus === "done" ? DesignTokens.badgeTeal : DesignTokens.textTertiary
                    }

                    Text {
                        Layout.fillWidth: true
                        text: tRow.album.length > 0 ? (tRow.artist + " · " + tRow.album) : tRow.artist
                        elide: Text.ElideRight
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                    }
                }
            }

            FormatBadge {
                compact: true
                visible: tRow.formatBadge.length > 0
                text: tRow.formatBadge
            }

            Text {
                Layout.preferredWidth: 44
                horizontalAlignment: Text.AlignRight
                text: root.controller.formatTime(tRow.durationMs)
                color: DesignTokens.textTertiary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 11
                font.features: ({ "tnum": 1 })
            }

            RowLayout {
                spacing: 2

                InlineIconBtn {
                    id: playNextBtn
                    visible: !root.compact && tRow.hovered
                    iconName: "play_next"
                    onClicked: {
                        if (tRow.detailMode) root.onlineCtrl.playNextDetailAt(tRow.index)
                        else root.onlineCtrl.playNextSongAt(tRow.index)
                    }
                }

                InlineIconBtn {
                    id: queueBtn
                    visible: !root.compact && tRow.hovered
                    iconName: "queue"
                    onClicked: {
                        if (tRow.detailMode) root.onlineCtrl.enqueueDetailAt(tRow.index)
                        else root.onlineCtrl.enqueueSongAt(tRow.index)
                    }
                }

                InlineIconBtn {
                    id: dlBtn
                    visible: tRow.hovered || tRow.downloadStatus !== "none"
                    iconName: tRow.downloadStatus === "done" ? "check" : "download"
                    activeAccent: tRow.downloadStatus === "done"
                    enabled: !tRow.isDlActive
                    opacity: tRow.isDlActive ? 0.55 : 1.0
                    SequentialAnimation on opacity {
                        running: tRow.isDlActive
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.35; to: 0.90; duration: 650; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.90; to: 0.35; duration: 650; easing.type: Easing.InOutSine }
                    }
                    onClicked: {
                        if (tRow.detailMode) root.onlineCtrl.downloadDetailAt(tRow.index)
                        else root.onlineCtrl.downloadSongAt(tRow.index)
                    }
                }

                InlineIconBtn {
                    id: infoBtn
                    visible: !root.compact && tRow.hovered
                    iconName: "info"
                    onClicked: {
                        const mdl = tRow.detailMode ? root.onlineCtrl.detailTracksModel : root.onlineCtrl.songsModel
                        root.inspectTrack(mdl.trackMapAt(tRow.index))
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 16 : (root.width >= 1560 ? 26 : 22)
        spacing: 14

        // Top Header Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            VirelaButton {
                visible: root.onlineCtrl.inDetail
                compact: true
                iconName: "chevron_left"
                text: qsTr("Back")
                onClicked: root.onlineCtrl.closeDetail()
            }

            Text {
                text: root.isDiscoverHome ? qsTr("Discover") : qsTr("Online Music")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 24
                font.weight: Font.Bold
                font.letterSpacing: -0.3
            }

            FormatBadge {
                visible: root.onlineCtrl.activeDownloadCount > 0
                text: qsTr("Downloading %1").arg(root.onlineCtrl.activeDownloadCount)
            }

            FormatBadge {
                visible: root.onlineCtrl.busy || (root.isDiscoverHome && root.onlineCtrl.discoverLoading)
                text: qsTr("Loading...")
            }

            Item { Layout.fillWidth: true }

        }

        // Segmented Category Tabs (Songs / Albums / Artists / Playlists) shown in Search mode
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.neteaseConfigured && root.isSearchView

            Repeater {
                model: [
                    { key: "songs",     label: qsTr("Songs") },
                    { key: "albums",    label: qsTr("Albums") },
                    { key: "artists",   label: qsTr("Artists") },
                    { key: "playlists", label: qsTr("Playlists") }
                ]
                delegate: Rectangle {
                    id: tabChip
                    required property var modelData
                    readonly property bool active: root.onlineCtrl.searchType === modelData.key
                    implicitWidth: chipLbl.implicitWidth + 24
                    implicitHeight: 30
                    radius: 15
                    color: active ? DesignTokens.surfaceSelected : (chipMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.surfaceInput)
                    border.width: 1
                    border.color: active
                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.42)
                        : DesignTokens.hairlineBorder

                    Text {
                        id: chipLbl
                        anchors.centerIn: parent
                        text: tabChip.modelData.label
                        color: tabChip.active ? DesignTokens.accentLight : DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.weight: tabChip.active ? Font.DemiBold : Font.Medium
                    }

                    MouseArea {
                        id: chipMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (!tabChip.active) root.onlineCtrl.search(root.onlineCtrl.searchQuery, tabChip.modelData.key)
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Error Banner
        Rectangle {
            Layout.fillWidth: true
            visible: root.onlineCtrl.backendConfigured && root.onlineCtrl.errorMessage.length > 0
            implicitHeight: errTxt.implicitHeight + 16
            radius: 10
            color: Qt.rgba(0.88, 0.24, 0.30, 0.14)
            border.width: 1
            border.color: Qt.rgba(0.88, 0.24, 0.30, 0.35)

            Text {
                id: errTxt
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                wrapMode: Text.WordWrap
                text: root.onlineCtrl.errorMessage
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 12
            }
        }

        // Local Library Match Bridge Banner
        Rectangle {
            Layout.fillWidth: true
            visible: !root.onlineCtrl.inDetail && root.onlineCtrl.searchQuery.length > 0 && root.localMatchCount > 0
            implicitHeight: 38
            radius: 10
            color: DesignTokens.accentSoft
            border.width: 1
            border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.30)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                spacing: 8

                IconGlyph { name: "music"; size: 14; color: DesignTokens.accent }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Found %1 matching track(s) in Local Library").arg(root.localMatchCount)
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                VirelaButton {
                    compact: true
                    text: qsTr("View Local")
                    onClicked: root.viewLocalMatchesRequested(root.onlineCtrl.searchQuery)
                }
            }
        }

        // Main Browse Area (Discover Home or Search Results when not in detail drill-down)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.onlineCtrl.inDetail

            // 0. Discover Home Landing View (lazy-loaded only when active)
            Loader {
                id: discoverLoader
                anchors.fill: parent
                active: root.isDiscoverHome
                visible: active
                sourceComponent: Flickable {
                id: discoverScroll
                anchors.fill: parent
                contentWidth: width
                contentHeight: discoverCol.implicitHeight + 28
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: discoverCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 22

                    // Subtle Online Source / Fallback Status Strip (non-blocking)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: (!root.onlineCtrl.backendConfigured && root.hasLocalDiscoverContent)
                            || (root.onlineCtrl.backendConfigured && !root.neteaseConfigured)
                            || (root.neteaseConfigured && root.onlineCtrl.discoverError.length > 0 && !root.hasDiscoverOnlineContent)
                        implicitHeight: Math.max(46, statusRow.implicitHeight + 16)
                        radius: DesignTokens.radiusMd
                        color: DesignTokens.stageSurfaceBg
                        border.width: 1
                        border.color: DesignTokens.hairlineBorder

                        RowLayout {
                            id: statusRow
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 10

                            IconGlyph {
                                name: "cloud"
                                size: 16
                                color: DesignTokens.accent
                            }

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: !root.onlineCtrl.backendConfigured
                                    ? qsTr("Online discovery is not configured yet. Showing your local music highlights below.")
                                    : (!root.neteaseConfigured
                                        ? qsTr("Meting source active. Use the top search bar to find tracks, or configure Netease API for personalized discovery.")
                                        : qsTr("Online discovery is temporarily unreachable (%1). Showing local music highlights.").arg(root.onlineCtrl.discoverError))
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                            }

                            VirelaButton {
                                visible: root.neteaseConfigured && root.onlineCtrl.discoverError.length > 0
                                compact: true
                                iconName: "refresh"
                                text: qsTr("Retry")
                                onClicked: root.onlineCtrl.refreshDiscover()
                            }

                            VirelaButton {
                                compact: true
                                iconName: "settings"
                                text: qsTr("Settings")
                                onClicked: root.openSettingsRequested()
                            }
                        }
                    }

                    // Dual-CTA Onboarding when both online discovery and local library are empty
                    GlassPanel {
                        Layout.fillWidth: true
                        visible: !root.hasDiscoverOnlineContent
                            && !root.hasLocalDiscoverContent
                            && !root.onlineCtrl.discoverLoading
                            && (root.lib.totalTrackCount === 0 || !root.onlineCtrl.backendConfigured)
                        implicitHeight: emptyOnboardCol.implicitHeight + 48
                        radius: DesignTokens.radiusXl
                        panelColor: DesignTokens.stageSurfaceBg
                        borderColor: DesignTokens.hairlineBorder
                        shadowEnabled: false

                        ColumnLayout {
                            id: emptyOnboardCol
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 48, 460)
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                implicitWidth: 60
                                implicitHeight: 60
                                radius: 18
                                color: DesignTokens.accentSoft
                                border.width: 1
                                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.28)
                                IconGlyph { anchors.centerIn: parent; name: "cloud"; size: 26; color: DesignTokens.accent }
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: !root.onlineCtrl.backendConfigured
                                    ? qsTr("Configure Your Online Music Sources")
                                    : qsTr("Welcome to Virela Discover")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: !root.onlineCtrl.backendConfigured
                                    ? qsTr("Connect a Netease or Meting music source to unlock trending charts and streaming playback, or add a local music folder.")
                                    : qsTr("Use the top search bar to look up songs, albums, and artists, or open a Netease playlist link directly.")
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                lineHeight: 1.45
                            }

                            Flow {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: Math.min(emptyOnboardCol.width, spacing
                                    + (discoverSettingsButton.visible ? discoverSettingsButton.implicitWidth : discoverSearchButton.implicitWidth)
                                    + discoverLocalButton.implicitWidth)
                                Layout.topMargin: 4
                                spacing: 10

                                VirelaButton {
                                    id: discoverSettingsButton
                                    visible: !root.onlineCtrl.backendConfigured
                                    primary: true
                                    iconName: "settings"
                                    text: qsTr("Open Online Source Settings")
                                    onClicked: root.openSettingsRequested()
                                }

                                VirelaButton {
                                    id: discoverSearchButton
                                    visible: root.onlineCtrl.backendConfigured
                                    primary: true
                                    iconName: "search"
                                    text: qsTr("Start Searching")
                                    onClicked: root.focusSearchRequested()
                                }

                                VirelaButton {
                                    id: discoverLocalButton
                                    iconName: "music"
                                    text: qsTr("Browse Local Music")
                                    onClicked: root.openLocalRequested()
                                }
                            }
                        }
                    }

                    // A. Continue Listening / Recently Added (Local Library Summary)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.lib.recentTracksSummary.length > 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconGlyph { name: "clock"; size: 15; color: DesignTokens.accent }

                            Text {
                                Layout.fillWidth: true
                                text: root.lib.hasRecentPlays ? qsTr("Continue Listening") : qsTr("Recently Added")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            VirelaButton {
                                compact: true
                                iconName: "chevron_right"
                                text: qsTr("Open Library")
                                onClicked: {
                                    if (root.lib.hasRecentPlays) root.openRecentRequested()
                                    else root.openLocalRequested()
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.cardGridColumns
                            columnSpacing: 10
                            rowSpacing: 8

                            Repeater {
                                model: root.lib.recentTracksSummary
                                delegate: Rectangle {
                                    id: recCard
                                    required property var modelData
                                    readonly property bool isOnlineItem: (modelData.source || "local") !== "local"
                                    readonly property bool isCurrent: isOnlineItem
                                        ? (root.controller.trackIsOnline && root.controller.trackKey === (modelData.trackKey || ""))
                                        : (!root.controller.trackIsOnline && root.controller.trackId === Number(modelData.id))

                                    Layout.fillWidth: true
                                    implicitHeight: 54
                                    radius: DesignTokens.radiusMd
                                    color: recCard.isCurrent
                                        ? DesignTokens.surfaceSelected
                                        : (recMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg)
                                    border.width: 1
                                    border.color: recCard.isCurrent
                                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.36)
                                        : (recMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder)
                                    Behavior on color { ColorAnimation { duration: 130 } }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        spacing: 10

                                        Rectangle {
                                            Layout.preferredWidth: 38
                                            Layout.preferredHeight: 38
                                            radius: 8
                                            color: DesignTokens.surfaceInput
                                            clip: true

                                            Image {
                                                id: recCov
                                                anchors.fill: parent
                                                source: recCard.modelData.coverHash && recCard.modelData.coverHash.length > 0
                                                    ? ("image://cover/" + recCard.modelData.coverHash + "?size=96")
                                                    : (recCard.modelData.coverUrl || "")
                                                sourceSize: Qt.size(80, 80)
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: false
                                                visible: status === Image.Ready
                                            }
                                            IconGlyph {
                                                anchors.centerIn: parent
                                                visible: recCov.status !== Image.Ready && !recMa.containsMouse
                                                name: "music"
                                                size: 16
                                                color: DesignTokens.textTertiary
                                            }
                                            Rectangle {
                                                anchors.fill: parent
                                                visible: recMa.containsMouse || recCard.isCurrent
                                                color: recCov.visible ? Qt.rgba(0, 0, 0, 0.44) : "transparent"
                                                IconGlyph {
                                                    anchors.centerIn: parent
                                                    name: recCard.isCurrent && root.controller.isPlaying ? "pause" : "play"
                                                    size: 14
                                                    color: recCov.visible ? "#ffffff" : DesignTokens.accent
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: recCard.modelData.title || ""
                                                elide: Text.ElideRight
                                                color: recCard.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                                font.family: DesignTokens.fontFamily
                                                font.pixelSize: 13
                                                font.weight: recCard.isCurrent ? Font.DemiBold : Font.Medium
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 4

                                                IconGlyph {
                                                    visible: recCard.isOnlineItem
                                                    name: "cloud"
                                                    size: 10
                                                    color: DesignTokens.accentLight
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: recCard.modelData.album && recCard.modelData.album.length > 0
                                                        ? (recCard.modelData.artist + " · " + recCard.modelData.album)
                                                        : (recCard.modelData.artist || "")
                                                    elide: Text.ElideRight
                                                    color: DesignTokens.textTertiary
                                                    font.family: DesignTokens.fontFamily
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }

                                        FormatBadge {
                                            compact: true
                                            visible: (recCard.modelData.formatBadge || "").length > 0
                                            text: recCard.modelData.formatBadge || ""
                                        }
                                    }

                                    MouseArea {
                                        id: recMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (recCard.isCurrent) root.controller.togglePlayPause()
                                            else root.lib.playTrackById(Number(recCard.modelData.id))
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // B. Discover Loading Placeholder (when fetching initially)
                    GlassPanel {
                        Layout.fillWidth: true
                        visible: root.neteaseConfigured
                            && root.onlineCtrl.discoverLoading
                            && !root.hasDiscoverOnlineContent
                        implicitHeight: 68
                        radius: DesignTokens.radiusLg
                        panelColor: DesignTokens.stageSurfaceBg
                        borderColor: DesignTokens.hairlineBorder
                        shadowEnabled: false

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12
                            IconGlyph { name: "cloud"; size: 18; color: DesignTokens.accent }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Loading curated playlists and new releases...")
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                            }
                        }
                    }

                    // D. Featured Playlists Carousel
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.neteaseConfigured && root.onlineCtrl.recommendedPlaylistsModel.count > 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconGlyph { name: "playlist"; size: 15; color: DesignTokens.accent }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Featured Playlists")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                        }

                        ListView {
                            id: featuredCarousel
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.carouselCardWidth + 66
                            orientation: ListView.Horizontal
                            spacing: 12
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: visible ? root.onlineCtrl.recommendedPlaylistsModel : null
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Item {
                                id: fPlCard
                                required property string playlistId
                                required property string name
                                required property string creator
                                required property string coverUrl
                                required property int trackCount
                                required property string description

                                width: root.carouselCardWidth
                                height: root.carouselCardWidth + 58

                                Rectangle {
                                    x: 0
                                    y: fPlMa.containsMouse ? 0 : 4
                                    width: parent.width
                                    height: parent.height - 8
                                    radius: DesignTokens.radiusLg
                                    color: fPlMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                                    border.width: 1
                                    border.color: fPlMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder
                                    Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 7

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: width
                                            radius: DesignTokens.radiusMd
                                            color: DesignTokens.surfaceInput
                                            clip: true

                                            Image {
                                                id: fPlImg
                                                anchors.fill: parent
                                                source: fPlCard.coverUrl
                                                sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: false
                                                visible: status === Image.Ready
                                            }
                                            IconGlyph {
                                                anchors.centerIn: parent
                                                name: "playlist"
                                                size: 32
                                                color: DesignTokens.textTertiary
                                                visible: fPlImg.status !== Image.Ready
                                            }
                                            FormatBadge {
                                                visible: fPlCard.trackCount > 0
                                                anchors.left: parent.left
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 6
                                                compact: true
                                                text: qsTr("%1 tracks").arg(fPlCard.trackCount)
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: fPlCard.name
                                            maximumLineCount: 2
                                            wrapMode: Text.WordWrap
                                            elide: Text.ElideRight
                                            color: DesignTokens.textPrimary
                                            font.family: DesignTokens.fontFamily
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    MouseArea {
                                        id: fPlMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.onlineCtrl.openPlaylist(
                                            fPlCard.playlistId, fPlCard.name, fPlCard.creator, fPlCard.coverUrl, fPlCard.description)
                                    }
                                }
                            }
                        }
                    }

                    // E. New Releases Grid (2-col)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.neteaseConfigured && root.onlineCtrl.newSongsModel.count > 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconGlyph { name: "music"; size: 15; color: DesignTokens.accent }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("New Releases")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.cardGridColumns
                            columnSpacing: 10
                            rowSpacing: 4

                            Repeater {
                                model: root.isDiscoverHome && root.neteaseConfigured ? root.onlineCtrl.newSongsModel : null
                                delegate: Rectangle {
                                    id: nsRow
                                    required property int index
                                    required property string title
                                    required property string artist
                                    required property string album
                                    required property int durationMs
                                    required property string formatBadge
                                    required property string coverUrl
                                    required property string trackKey
                                    required property string downloadStatus

                                    readonly property bool isCurrent: root.controller.trackIsOnline
                                        && root.controller.trackKey === nsRow.trackKey
                                    readonly property bool isDlActive: nsRow.downloadStatus === "downloading" || nsRow.downloadStatus === "queued"
                                    readonly property bool hovered: nsMa.containsMouse || nsThumbMa.containsMouse
                                        || nsNextBtn.hovered || nsQueueBtn.hovered || nsDlBtn.hovered

                                    Layout.fillWidth: true
                                    implicitHeight: 50
                                    radius: DesignTokens.radiusSm
                                    color: isCurrent
                                        ? DesignTokens.surfaceSelected
                                        : (hovered ? DesignTokens.surfaceHover : DesignTokens.stageSurfaceBg)
                                    border.width: 1
                                    border.color: isCurrent
                                        ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.30)
                                        : DesignTokens.hairlineBorder
                                    Behavior on color { ColorAnimation { duration: 110 } }

                                    MouseArea {
                                        id: nsMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.PointingHandCursor
                                        onDoubleClicked: root.onlineCtrl.playNewSongAt(nsRow.index)
                                        onClicked: (mouse) => {
                                            if (mouse.button === Qt.RightButton) {
                                                songContextMenu.openFor(nsRow.index, "newsongs", root.onlineCtrl.newSongsModel.trackMapAt(nsRow.index))
                                            }
                                        }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 8
                                        spacing: 10

                                        Rectangle {
                                            Layout.preferredWidth: 34
                                            Layout.preferredHeight: 34
                                            radius: 7
                                            color: DesignTokens.surfaceInput
                                            clip: true

                                            Image {
                                                id: nsCov
                                                anchors.fill: parent
                                                source: nsRow.coverUrl
                                                sourceSize: Qt.size(80, 80)
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: false
                                                visible: status === Image.Ready
                                            }
                                            IconGlyph {
                                                anchors.centerIn: parent
                                                visible: nsCov.status !== Image.Ready && !nsThumbMa.containsMouse
                                                name: "music"
                                                size: 15
                                                color: DesignTokens.textTertiary
                                            }
                                            Rectangle {
                                                anchors.fill: parent
                                                visible: nsThumbMa.containsMouse || nsRow.isCurrent
                                                color: nsCov.visible ? Qt.rgba(0, 0, 0, 0.45) : "transparent"
                                                IconGlyph {
                                                    anchors.centerIn: parent
                                                    name: nsRow.isCurrent && root.controller.isPlaying ? "pause" : "play"
                                                    size: 13
                                                    color: nsCov.visible ? "#ffffff" : DesignTokens.accent
                                                }
                                            }
                                            MouseArea {
                                                id: nsThumbMa
                                                anchors.fill: parent
                                                z: 4
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (nsRow.isCurrent) root.controller.togglePlayPause()
                                                    else root.onlineCtrl.playNewSongAt(nsRow.index)
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 1

                                            Text {
                                                Layout.fillWidth: true
                                                text: nsRow.title
                                                elide: Text.ElideRight
                                                color: nsRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                                font.family: DesignTokens.fontFamily
                                                font.pixelSize: 13
                                                font.weight: nsRow.isCurrent ? Font.DemiBold : Font.Medium
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: nsRow.album.length > 0 ? (nsRow.artist + " · " + nsRow.album) : nsRow.artist
                                                elide: Text.ElideRight
                                                color: DesignTokens.textTertiary
                                                font.family: DesignTokens.fontFamily
                                                font.pixelSize: 11
                                            }
                                        }

                                        FormatBadge {
                                            compact: true
                                            visible: !nsRow.hovered && nsRow.formatBadge.length > 0
                                            text: nsRow.formatBadge
                                        }

                                        RowLayout {
                                            spacing: 2
                                            InlineIconBtn {
                                                id: nsNextBtn
                                                visible: nsRow.hovered
                                                iconName: "play_next"
                                                onClicked: root.onlineCtrl.playNextNewSongAt(nsRow.index)
                                            }
                                            InlineIconBtn {
                                                id: nsQueueBtn
                                                visible: nsRow.hovered
                                                iconName: "queue"
                                                onClicked: root.onlineCtrl.enqueueNewSongAt(nsRow.index)
                                            }
                                            InlineIconBtn {
                                                id: nsDlBtn
                                                visible: nsRow.hovered || nsRow.downloadStatus !== "none"
                                                iconName: nsRow.downloadStatus === "done" ? "check" : "download"
                                                activeAccent: nsRow.downloadStatus === "done"
                                                enabled: !nsRow.isDlActive
                                                opacity: nsRow.isDlActive ? 0.55 : 1.0
                                                onClicked: root.onlineCtrl.downloadNewSongAt(nsRow.index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // F. Local Albums Showcase Fallback (when Netease is unconfigured or discovery unreachable)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: (!root.neteaseConfigured || (!root.hasDiscoverOnlineContent && root.onlineCtrl.discoverError.length > 0))
                            && root.lib.recentAlbumsSummary.length > 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            IconGlyph { name: "album"; size: 15; color: DesignTokens.accent }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Local Albums")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            VirelaButton {
                                compact: true
                                iconName: "chevron_right"
                                text: qsTr("Browse All")
                                onClicked: root.openLocalRequested()
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.carouselCardWidth + 66
                            orientation: ListView.Horizontal
                            spacing: 12
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: visible ? root.lib.recentAlbumsSummary : null
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Item {
                                id: locAlCard
                                required property var modelData
                                width: root.carouselCardWidth
                                height: root.carouselCardWidth + 58

                                Rectangle {
                                    x: 0
                                    y: locAlMa.containsMouse ? 0 : 4
                                    width: parent.width
                                    height: parent.height - 8
                                    radius: DesignTokens.radiusLg
                                    color: locAlMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                                    border.width: 1
                                    border.color: locAlMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder
                                    Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 7

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: width
                                            radius: DesignTokens.radiusMd
                                            color: DesignTokens.surfaceInput
                                            clip: true

                                            Image {
                                                id: locAlImg
                                                anchors.fill: parent
                                                source: locAlCard.modelData.coverHash && locAlCard.modelData.coverHash.length > 0
                                                    ? ("image://cover/" + locAlCard.modelData.coverHash + "?size=200")
                                                    : ""
                                                sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: false
                                                visible: status === Image.Ready
                                            }
                                            IconGlyph {
                                                anchors.centerIn: parent
                                                name: "album"
                                                size: 32
                                                color: DesignTokens.textTertiary
                                                visible: locAlImg.status !== Image.Ready
                                            }
                                            FormatBadge {
                                                visible: (locAlCard.modelData.trackCount || 0) > 0
                                                anchors.left: parent.left
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 6
                                                compact: true
                                                text: qsTr("%1 tracks").arg(locAlCard.modelData.trackCount || 0)
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: locAlCard.modelData.album || ""
                                            elide: Text.ElideRight
                                            color: DesignTokens.textPrimary
                                            font.family: DesignTokens.fontFamily
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: locAlCard.modelData.artist || ""
                                            elide: Text.ElideRight
                                            color: DesignTokens.textTertiary
                                            font.family: DesignTokens.fontFamily
                                            font.pixelSize: 11
                                        }
                                    }

                                    MouseArea {
                                        id: locAlMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.openLocalAlbumRequested(
                                            locAlCard.modelData.album || "",
                                            locAlCard.modelData.artist || "",
                                            locAlCard.modelData.coverHash || "")
                                    }
                                }
                            }
                        }
                    }
                }
                }
            }

            // 1–4. Search Results Views (lazy-loaded only during active search)
            Loader {
                id: searchResultsLoader
                anchors.fill: parent
                active: root.isSearchView
                visible: active
                sourceComponent: Item {
                    anchors.fill: parent

                    // 1. Songs Tab (Search Mode)
                    ListView {
                        id: songsList
                        anchors.fill: parent
                        visible: root.onlineCtrl.searchType === "songs"
                clip: true
                spacing: 3
                reuseItems: true
                cacheBuffer: 120
                boundsBehavior: Flickable.StopAtBounds
                model: songsList.visible ? root.onlineCtrl.songsModel : null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: TrackListDelegate {
                    detailMode: false
                }
            }

            // 2. Albums Grid Tab
            GridView {
                id: albumGrid
                anchors.fill: parent
                visible: root.onlineCtrl.searchType === "albums"
                clip: true
                readonly property int cols: Math.max(1, Math.floor(width / root.searchCardBaseWidth))
                cellWidth: Math.max(1, Math.floor(width / cols))
                cellHeight: cellWidth + 54
                cacheBuffer: 120
                boundsBehavior: Flickable.StopAtBounds
                model: albumGrid.visible ? root.onlineCtrl.albumsModel : null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: alCard
                    required property string albumId
                    required property string name
                    required property string artist
                    required property string coverUrl
                    required property int year

                    width: albumGrid.cellWidth
                    height: albumGrid.cellHeight

                    Rectangle {
                        x: 7
                        y: alMa.containsMouse ? 2 : 7
                        width: parent.width - 14
                        height: parent.height - 14
                        radius: DesignTokens.radiusLg
                        color: alMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                        border.width: 1
                        border.color: alMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder
                        Behavior on y { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 7

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: width
                                radius: DesignTokens.radiusMd
                                color: DesignTokens.surfaceInput
                                clip: true

                                Image {
                                    id: alImg
                                    anchors.fill: parent
                                    source: alCard.coverUrl
                                    sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                    visible: status === Image.Ready
                                }
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "album"
                                    size: 36
                                    color: DesignTokens.textTertiary
                                    visible: alImg.status !== Image.Ready
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: alCard.name
                                elide: Text.ElideRight
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: alCard.year > 0 ? (alCard.artist + " · " + alCard.year) : alCard.artist
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: alMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.onlineCtrl.openAlbum(alCard.albumId, alCard.name, alCard.artist, alCard.coverUrl)
                        }
                    }
                }
            }

            // 3. Artists Grid Tab
            GridView {
                id: artistGrid
                anchors.fill: parent
                visible: root.onlineCtrl.searchType === "artists"
                clip: true
                readonly property int cols: Math.max(1, Math.floor(width / (root.searchCardBaseWidth - 8)))
                cellWidth: Math.max(1, Math.floor(width / cols))
                cellHeight: cellWidth + 46
                cacheBuffer: 120
                boundsBehavior: Flickable.StopAtBounds
                model: artistGrid.visible ? root.onlineCtrl.artistsModel : null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: arCard
                    required property string artistId
                    required property string name
                    required property string coverUrl
                    required property int albumCount
                    required property int trackCount

                    width: artistGrid.cellWidth
                    height: artistGrid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 7
                        radius: DesignTokens.radiusLg
                        color: arMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                        border.width: 1
                        border.color: arMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: width
                                radius: width * 0.5
                                color: DesignTokens.surfaceInput
                                clip: true

                                Image {
                                    id: arImg
                                    anchors.fill: parent
                                    source: arCard.coverUrl
                                    sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                    visible: status === Image.Ready
                                }
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "artist"
                                    size: 38
                                    color: DesignTokens.textTertiary
                                    visible: arImg.status !== Image.Ready
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: arCard.name
                                elide: Text.ElideRight
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("%1 albums · %2 tracks").arg(arCard.albumCount).arg(arCard.trackCount)
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: arMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.onlineCtrl.openArtist(arCard.artistId, arCard.name, arCard.coverUrl)
                        }
                    }
                }
            }

            // 4. Playlists Grid Tab
            GridView {
                id: playlistGrid
                anchors.fill: parent
                visible: root.onlineCtrl.searchType === "playlists"
                clip: true
                readonly property int cols: Math.max(1, Math.floor(width / root.searchCardBaseWidth))
                cellWidth: Math.max(1, Math.floor(width / cols))
                cellHeight: cellWidth + 54
                cacheBuffer: 120
                boundsBehavior: Flickable.StopAtBounds
                model: playlistGrid.visible ? root.onlineCtrl.playlistsModel : null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: plCard
                    required property string playlistId
                    required property string name
                    required property string creator
                    required property string coverUrl
                    required property int trackCount
                    required property string description

                    width: playlistGrid.cellWidth
                    height: playlistGrid.cellHeight

                    Rectangle {
                        x: 7
                        y: plMa.containsMouse ? 2 : 7
                        width: parent.width - 14
                        height: parent.height - 14
                        radius: DesignTokens.radiusLg
                        color: plMa.containsMouse ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                        border.width: 1
                        border.color: plMa.containsMouse ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder
                        Behavior on y { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 7

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: width
                                radius: DesignTokens.radiusMd
                                color: DesignTokens.surfaceInput
                                clip: true

                                Image {
                                    id: plImg
                                    anchors.fill: parent
                                    source: plCard.coverUrl
                                    sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                    visible: status === Image.Ready
                                }
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "playlist"
                                    size: 36
                                    color: DesignTokens.textTertiary
                                    visible: plImg.status !== Image.Ready
                                }
                                FormatBadge {
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 7
                                    compact: true
                                    text: qsTr("%1 tracks").arg(plCard.trackCount)
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: plCard.name
                                elide: Text.ElideRight
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: plCard.creator.length > 0 ? plCard.creator : qsTr("Online Playlist")
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: plMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.onlineCtrl.openPlaylist(
                                plCard.playlistId, plCard.name, plCard.creator, plCard.coverUrl, plCard.description)
                        }
                    }
                }
            }

            // Empty Search Result Card (Search mode only)
            GlassPanel {
                anchors.centerIn: parent
                visible: {
                    if (!root.isSearchView) return false
                    const st = root.onlineCtrl.searchType
                    const count = st === "songs" ? root.onlineCtrl.songsModel.count
                        : (st === "albums" ? root.onlineCtrl.albumsModel.count
                        : (st === "artists" ? root.onlineCtrl.artistsModel.count
                        : root.onlineCtrl.playlistsModel.count))
                    return count === 0 && !root.onlineCtrl.busy
                        && (!root.onlineCtrl.backendConfigured || root.onlineCtrl.errorMessage.length === 0)
                }
                width: Math.min(parent.width - 48, 460)
                height: onboardCol.implicitHeight + 48
                radius: DesignTokens.radiusXl
                panelColor: DesignTokens.stageSurfaceBg
                borderColor: DesignTokens.hairlineBorder
                shadowEnabled: false

                ColumnLayout {
                    id: onboardCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 12

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 60
                        implicitHeight: 60
                        radius: 18
                        color: DesignTokens.accentSoft
                        border.width: 1
                        border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.28)
                        IconGlyph { anchors.centerIn: parent; name: "cloud"; size: 26; color: DesignTokens.accent }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: !root.onlineCtrl.backendConfigured ? qsTr("Configure Your Online Music Sources")
                            : (root.onlineCtrl.searchQuery.length > 0 ? qsTr("No results") : qsTr("Find music online"))
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: !root.onlineCtrl.backendConfigured
                            ? qsTr("Connect a Netease or Meting music source to search streaming tracks, or browse your local lossless library directly.")
                            : (root.onlineCtrl.searchQuery.length > 0
                                ? qsTr("Try a different keyword or category.")
                                : (root.neteaseConfigured
                                    ? qsTr("Search songs, albums, artists, or playlists above, or paste a Netease playlist link directly.")
                                    : qsTr("Use the top search bar to find songs.")))
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        lineHeight: 1.45
                    }

                    Flow {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(onboardCol.width, searchLocalButton.implicitWidth
                            + (searchSettingsButton.visible ? spacing + searchSettingsButton.implicitWidth
                                : (startSearchButton.visible ? spacing + startSearchButton.implicitWidth : 0)))
                        Layout.topMargin: 4
                        spacing: 10

                        VirelaButton {
                            id: searchSettingsButton
                            visible: !root.onlineCtrl.backendConfigured
                            primary: true
                            iconName: "settings"
                            text: qsTr("Open Online Source Settings")
                            onClicked: root.openSettingsRequested()
                        }

                        VirelaButton {
                            id: startSearchButton
                            visible: root.onlineCtrl.backendConfigured && root.onlineCtrl.searchQuery.length === 0
                            primary: true
                            iconName: "search"
                            text: qsTr("Start Searching")
                            onClicked: root.focusSearchRequested()
                        }

                        VirelaButton {
                            id: searchLocalButton
                            iconName: "music"
                            text: qsTr("Browse Local Music")
                            onClicked: root.openLocalRequested()
                        }
                    }
                }
            }
                }
            }
        }

        // Drill-down Detail View (Album / Artist / Playlist, lazy-loaded when active)
        Loader {
            id: detailLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: root.onlineCtrl.inDetail
            visible: active
            sourceComponent: ColumnLayout {
                anchors.fill: parent
                spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(root.compact ? 124 : 146, detailHeaderRow.implicitHeight + 28)
                radius: DesignTokens.radiusLg
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: DesignTokens.accentSoft }
                    GradientStop { position: 0.7; color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.03) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                border.width: 1
                border.color: DesignTokens.hairlineBorder

                RowLayout {
                    id: detailHeaderRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 14
                    spacing: 16

                    Rectangle {
                        id: detailCoverFrame
                        Layout.preferredWidth: root.compact ? 96 : 118
                        Layout.preferredHeight: root.compact ? 96 : 118
                        radius: root.onlineCtrl.detailType === "artist" ? width * 0.5 : DesignTokens.radiusMd
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: DesignTokens.borderHighlight
                        clip: true

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 18
                            height: parent.height + 18
                            radius: parent.radius + 9
                            color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.14)
                            z: -1
                        }

                        Image {
                            id: detailHeroImg
                            anchors.fill: parent
                            source: root.onlineCtrl.detailCoverUrl
                            sourceSize: Qt.size(root.cardCoverPx, root.cardCoverPx)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            visible: status === Image.Ready
                        }
                        IconGlyph {
                            anchors.centerIn: parent
                            name: root.onlineCtrl.detailType === "artist" ? "artist" : (root.onlineCtrl.detailType === "album" ? "album" : "playlist")
                            size: 38
                            color: DesignTokens.textTertiary
                            visible: detailHeroImg.status !== Image.Ready
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            Layout.fillWidth: true
                            text: root.onlineCtrl.detailTitle
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 21
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.onlineCtrl.detailSubtitle.length > 0
                            text: root.onlineCtrl.detailSubtitle
                            elide: Text.ElideRight
                            color: DesignTokens.accentLight
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.onlineCtrl.detailDescription.length > 0
                            text: root.onlineCtrl.detailDescription
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 8

                            VirelaButton {
                                primary: true
                                compact: true
                                iconName: "play"
                                enabled: root.onlineCtrl.detailTracksModel.count > 0
                                text: qsTr("Play All")
                                onClicked: root.onlineCtrl.playAllDetail()
                            }

                            VirelaButton {
                                compact: true
                                iconName: "queue"
                                enabled: root.onlineCtrl.detailTracksModel.count > 0
                                text: qsTr("Queue All")
                                onClicked: root.onlineCtrl.enqueueAllDetail()
                            }

                            VirelaButton {
                                compact: true
                                iconName: "download"
                                enabled: root.onlineCtrl.detailTracksModel.count > 0
                                text: qsTr("Download All")
                                onClicked: root.onlineCtrl.downloadAllDetail()
                            }

                            VirelaButton {
                                visible: root.onlineCtrl.detailType === "playlist" && root.onlineCtrl.detailRemoteId.length > 0
                                compact: true
                                primary: root.detailPlaylistPinned
                                iconName: root.detailPlaylistPinned ? "check" : "playlist"
                                text: root.detailPlaylistPinned ? qsTr("Pinned") : qsTr("Pin to Sidebar")
                                onClicked: {
                                    if (root.detailPlaylistPinned) {
                                        root.lib.unpinOnlinePlaylist("netease", root.onlineCtrl.detailRemoteId)
                                    } else {
                                        root.lib.pinOnlinePlaylist(
                                            "netease",
                                            root.onlineCtrl.detailRemoteId,
                                            root.onlineCtrl.detailTitle,
                                            root.onlineCtrl.detailSubtitle,
                                            root.onlineCtrl.detailCoverUrl,
                                            root.onlineCtrl.detailTracksModel.count)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                reuseItems: true
                cacheBuffer: 120
                boundsBehavior: Flickable.StopAtBounds
                model: visible ? root.onlineCtrl.detailTracksModel : null
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: TrackListDelegate {
                    detailMode: true
                }
            }
            }
        }
    }

    Menu {
        id: songContextMenu
        property int targetIndex: -1
        property string targetSource: "songs" // "songs" | "detail" | "newsongs"
        property var targetTrackMap: ({})
        property bool targetIsFav: false
        padding: 6

        function openFor(idx, src, map) {
            targetIndex = idx
            targetSource = src
            targetTrackMap = map || ({})
            const s = (targetTrackMap.source || "netease")
            const rid = (targetTrackMap.remoteId || "")
            targetIsFav = rid.length > 0 ? root.lib.isTrackFavorite(s, rid) : Boolean(targetTrackMap.isFavorite)
            popup()
        }

        background: GlassPanel {
            implicitWidth: 208
            radius: DesignTokens.radiusMd
            elevated: true
            shadowEnabled: true
        }

        StyledMenuItem {
            text: qsTr("Play Now")
            onTriggered: {
                if (songContextMenu.targetSource === "detail") root.onlineCtrl.playDetailAt(songContextMenu.targetIndex)
                else if (songContextMenu.targetSource === "newsongs") root.onlineCtrl.playNewSongAt(songContextMenu.targetIndex)
                else root.onlineCtrl.playSongAt(songContextMenu.targetIndex)
            }
        }
        StyledMenuItem {
            text: qsTr("Play Next")
            onTriggered: {
                if (songContextMenu.targetSource === "detail") root.onlineCtrl.playNextDetailAt(songContextMenu.targetIndex)
                else if (songContextMenu.targetSource === "newsongs") root.onlineCtrl.playNextNewSongAt(songContextMenu.targetIndex)
                else root.onlineCtrl.playNextSongAt(songContextMenu.targetIndex)
            }
        }
        StyledMenuItem {
            text: qsTr("Add to Queue")
            onTriggered: {
                if (songContextMenu.targetSource === "detail") root.onlineCtrl.enqueueDetailAt(songContextMenu.targetIndex)
                else if (songContextMenu.targetSource === "newsongs") root.onlineCtrl.enqueueNewSongAt(songContextMenu.targetIndex)
                else root.onlineCtrl.enqueueSongAt(songContextMenu.targetIndex)
            }
        }
        StyledMenuItem {
            text: songContextMenu.targetIsFav ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            onTriggered: root.lib.toggleTrackFavorite(songContextMenu.targetTrackMap)
        }
        Menu {
            id: onlinePlaylistSubMenu
            title: qsTr("Add to Playlist")
            enabled: root.lib.playlists.length > 0
            padding: 6

            background: GlassPanel {
                implicitWidth: 190
                radius: DesignTokens.radiusMd
                elevated: true
                shadowEnabled: true
            }

            Repeater {
                model: root.lib.playlists
                StyledMenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: root.lib.addTrackMapToPlaylist(modelData.id, songContextMenu.targetTrackMap)
                }
            }
        }
        StyledMenuItem {
            text: qsTr("Download to Library")
            onTriggered: {
                if (songContextMenu.targetSource === "detail") root.onlineCtrl.downloadDetailAt(songContextMenu.targetIndex)
                else if (songContextMenu.targetSource === "newsongs") root.onlineCtrl.downloadNewSongAt(songContextMenu.targetIndex)
                else root.onlineCtrl.downloadSongAt(songContextMenu.targetIndex)
            }
        }
        StyledMenuItem {
            text: qsTr("Track Technical Info")
            onTriggered: root.inspectTrack(songContextMenu.targetTrackMap)
        }
    }
}
