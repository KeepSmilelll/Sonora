import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Virela.UI

Item {
    id: root

    required property var controller
    required property var lib
    property var heroLayer: null

    property bool inDetail: false
    property string selectedAlbum: ""
    property string selectedArtist: ""
    property string selectedCoverHash: ""
    property int selectedYear: 0

    property int lastOpenedIndex: -1
    property var lastSourceRectInHero: null

    ParallelAnimation {
        id: gridRevealAnim
        NumberAnimation { target: gridPane; property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: gridShift; property: "x"; from: -16; to: 0; duration: 240; easing.type: Easing.OutQuint }
    }

    ParallelAnimation {
        id: detailRevealAnim
        NumberAnimation { target: detailMetaCol; property: "opacity"; from: 0.0; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
        NumberAnimation { target: detailMetaShift; property: "y"; from: 14; to: 0; duration: 260; easing.type: Easing.OutQuint }
        NumberAnimation { target: detailTrackList; property: "opacity"; from: 0.0; to: 1.0; duration: 240; easing.type: Easing.OutCubic }
        NumberAnimation { target: detailListShift; property: "y"; from: 18; to: 0; duration: 280; easing.type: Easing.OutQuint }
    }

    // Album Grid View
    Item {
        id: gridPane
        visible: !root.inDetail
        anchors.fill: parent
        anchors.margins: 22
        transform: Translate { id: gridShift; x: 0 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("Albums")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 24
                font.weight: Font.Bold
                font.letterSpacing: -0.3
            }

            Rectangle {
                implicitWidth: albumCountLbl.implicitWidth + 18
                implicitHeight: 24
                radius: 12
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: DesignTokens.hairlineBorder

                Text {
                    id: albumCountLbl
                    anchors.centerIn: parent
                    text: qsTr("%1 album(s)").arg(root.lib.albumsModel.count)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    font.features: ({ "tnum": 1 })
                }
            }

            Item { Layout.fillWidth: true }
        }

        GridView {
            id: albumGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            readonly property int columns: Math.max(2, Math.floor((Math.max(360, width) - 8) / 188))
            cellWidth: Math.floor((Math.max(360, width) - 8) / columns)
            cellHeight: cellWidth + 56
            cacheBuffer: 360
            boundsBehavior: Flickable.StopAtBounds
            model: root.lib.albumsModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: card
                required property int index
                required property string album
                required property string albumArtist
                required property int year
                required property int trackCount
                required property string coverHash
                required property string formatBadge

                width: albumGrid.cellWidth
                height: albumGrid.cellHeight
                readonly property bool hovered: cardMa.containsMouse || quickPlayMa.containsMouse

                Rectangle {
                    id: cardSurface
                    x: 7
                    y: card.hovered ? 2 : 7
                    width: parent.width - 14
                    height: parent.height - 14
                    radius: DesignTokens.radiusLg
                    color: card.hovered ? DesignTokens.surfaceCardHover : DesignTokens.stageSurfaceBg
                    border.width: 1
                    border.color: card.hovered ? DesignTokens.borderHighlight : DesignTokens.hairlineBorder

                    Behavior on y { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 7

                        Rectangle {
                            id: coverBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            radius: DesignTokens.radiusMd
                            color: DesignTokens.surfaceInput
                            border.width: 1
                            border.color: DesignTokens.hairlineBorder
                            clip: true
                            opacity: (root.heroLayer && root.heroLayer.isConcealing("album-cover")
                                      && root.lastOpenedIndex === card.index) ? 0.0 : 1.0

                            Rectangle {
                                id: gridCoverMask
                                anchors.fill: parent
                                radius: DesignTokens.radiusMd
                                visible: false
                                layer.enabled: true
                                layer.smooth: true
                            }

                            Image {
                                id: gridCoverImg
                                anchors.fill: parent
                                source: card.coverHash.length > 0 ? ("image://cover/" + card.coverHash + "?size=280") : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: false
                            }

                            MultiEffect {
                                anchors.fill: gridCoverImg
                                visible: gridCoverImg.status === Image.Ready
                                source: gridCoverImg
                                maskEnabled: true
                                maskSource: gridCoverMask
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                                scale: card.hovered ? 1.045 : 1.0
                                Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                            }

                            IconGlyph {
                                anchors.centerIn: parent
                                name: "album"
                                size: 42
                                color: DesignTokens.textTertiary
                                visible: gridCoverImg.status !== Image.Ready
                            }

                            FormatBadge {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.margins: 7
                                compact: true
                                text: card.formatBadge
                            }

                            // Hover Quick-Play Accent Circle
                            Item {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 8
                                width: 36
                                height: 36
                                z: 5
                                opacity: card.hovered ? 1.0 : 0.0
                                scale: quickPlayMa.pressed
                                    ? 0.90
                                    : (quickPlayMa.containsMouse ? 1.10 : (card.hovered ? 1.0 : 0.72))

                                Behavior on opacity { NumberAnimation { duration: 150 } }
                                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 1.15 } }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: 21
                                    color: Qt.rgba(0, 0, 0, 0.32)
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 18
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: DesignTokens.accentLight }
                                        GradientStop { position: 1.0; color: DesignTokens.accent }
                                    }

                                    IconGlyph {
                                        anchors.centerIn: parent
                                        anchors.horizontalCenterOffset: 1
                                        name: "play"
                                        size: 15
                                        color: "#ffffff"
                                    }
                                }

                                MouseArea {
                                    id: quickPlayMa
                                    anchors.fill: parent
                                    enabled: card.hovered
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.lib.openAlbum(card.album, card.albumArtist)
                                        root.lib.playFromDetailModel(0)
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.album
                            elide: Text.ElideRight
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.year > 0 ? (card.albumArtist + " · " + card.year) : card.albumArtist
                            elide: Text.ElideRight
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        id: cardMa
                        anchors.fill: parent
                        z: -1
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.lastOpenedIndex = card.index
                            const coverUrl = card.coverHash.length > 0 ? ("image://cover/" + card.coverHash + "?size=360") : ""
                            if (root.heroLayer) {
                                const srcPt = coverBox.mapToItem(root.heroLayer, 0, 0)
                                root.lastSourceRectInHero = {
                                    x: srcPt.x,
                                    y: srcPt.y,
                                    width: coverBox.width,
                                    height: coverBox.height
                                }
                                // Deterministic detailCoverBox destination: (36, 80, 148, 148) inside root
                                const destPt = root.mapToItem(root.heroLayer, 36, 80)
                                root.heroLayer.startHero({
                                    tag: "album-cover",
                                    sourceItem: coverBox,
                                    fromRadius: DesignTokens.radiusMd,
                                    toRect: {
                                        x: destPt.x,
                                        y: destPt.y,
                                        width: 148,
                                        height: 148
                                    },
                                    toRadius: DesignTokens.radiusLg,
                                    imageUrl: coverUrl,
                                    duration: 380
                                })
                            }
                            root.selectedAlbum = card.album
                            root.selectedArtist = card.albumArtist
                            root.selectedCoverHash = card.coverHash
                            root.selectedYear = card.year
                            root.lib.openAlbum(card.album, card.albumArtist)
                            root.inDetail = true
                            detailRevealAnim.restart()
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                visible: root.lib.albumsModel.count === 0
                spacing: 8
                IconGlyph {
                    Layout.alignment: Qt.AlignHCenter
                    name: "album"; size: 36; color: DesignTokens.textTertiary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No albums indexed yet")
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 14
                }
            }
        }
        }
    }

    // Album Detail View (Deterministic header geometry for Hero alignment)
    ColumnLayout {
        id: detailPane
        visible: root.inDetail
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 8

            VirelaButton {
                compact: true
                iconName: "chevron_left"
                text: qsTr("Albums")
                onClicked: {
                    if (root.heroLayer && root.lastSourceRectInHero) {
                        const coverUrl = root.selectedCoverHash.length > 0
                            ? ("image://cover/" + root.selectedCoverHash + "?size=360") : ""
                        root.heroLayer.startHero({
                            tag: "album-cover",
                            sourceItem: detailCoverBox,
                            fromRadius: DesignTokens.radiusLg,
                            toRect: root.lastSourceRectInHero,
                            toRadius: DesignTokens.radiusMd,
                            imageUrl: coverUrl,
                            duration: 340
                        })
                    }
                    root.inDetail = false
                    gridRevealAnim.restart()
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Borderless atmospheric detail hero header (top at y = 22 + 30 + 14 = 66 in root)
        Item {
            id: detailHeroHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 176

            // Soft atmospheric gradient wash
            Rectangle {
                anchors.fill: parent
                radius: DesignTokens.radiusLg
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: DesignTokens.accentSoft }
                    GradientStop { position: 0.65; color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.03) }
                    GradientStop { position: 1.0; color: "transparent" }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: DesignTokens.hairlineBorder
                }
            }

            // Soft ambient halo behind cover
            Rectangle {
                anchors.centerIn: detailCoverBox
                width: detailCoverBox.width + 26
                height: detailCoverBox.height + 26
                radius: DesignTokens.radiusXl
                color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.14 : 0.09)
                opacity: (root.heroLayer && root.heroLayer.isConcealing("album-cover")) ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 180 } }
            }

            // Exact deterministic Hero landing box at (14, 14) in detailHeroHeader -> (36, 80) in root
            Rectangle {
                id: detailCoverBox
                x: 14
                y: 14
                width: 148
                height: 148
                radius: DesignTokens.radiusLg
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: DesignTokens.borderHighlight
                clip: true
                opacity: (root.heroLayer && root.heroLayer.isConcealing("album-cover")) ? 0.0 : 1.0

                Rectangle {
                    id: detailCoverMask
                    anchors.fill: parent
                    radius: DesignTokens.radiusLg
                    visible: false
                    layer.enabled: true
                    layer.smooth: true
                }

                Image {
                    id: detailCoverImg
                    anchors.fill: parent
                    source: root.selectedCoverHash.length > 0 ? ("image://cover/" + root.selectedCoverHash + "?size=360") : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: false
                }

                MultiEffect {
                    anchors.fill: detailCoverImg
                    visible: detailCoverImg.status === Image.Ready
                    source: detailCoverImg
                    maskEnabled: true
                    maskSource: detailCoverMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "album"
                    size: 46
                    color: DesignTokens.textTertiary
                    visible: detailCoverImg.status !== Image.Ready
                }
            }

            Item {
                id: detailMetaCol
                anchors.left: detailCoverBox.right
                anchors.leftMargin: 22
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: detailCoverBox.verticalCenter
                height: detailMetaInner.implicitHeight
                transform: Translate { id: detailMetaShift; y: 0 }

                ColumnLayout {
                    id: detailMetaInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: root.selectedAlbum
                    elide: Text.ElideRight
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    font.letterSpacing: -0.3
                }

                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: root.selectedYear > 0 ? (root.selectedArtist + " · " + root.selectedYear) : root.selectedArtist
                    color: DesignTokens.accentLight
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.topMargin: 6
                    spacing: 10

                    VirelaButton {
                        primary: true
                        iconName: "play"
                        text: qsTr("Play Album")
                        onClicked: root.lib.playFromDetailModel(0)
                    }

                    Rectangle {
                        implicitWidth: dtCountLbl.implicitWidth + 18
                        implicitHeight: 28
                        radius: 14
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: DesignTokens.hairlineBorder

                        Text {
                            id: dtCountLbl
                            anchors.centerIn: parent
                            text: qsTr("%1 track(s)").arg(root.lib.detailTracksModel.count)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            font.features: ({ "tnum": 1 })
                        }
                    }
                }
                }
            }
        }

        ListView {
            id: detailTrackList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.lib.detailTracksModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            transform: Translate { id: detailListShift; y: 0 }

            delegate: Rectangle {
                id: dtRow
                required property int index
                required property int trackId
                required property int trackNumber
                required property string title
                required property string artist
                required property int durationMs
                required property string formatBadge

                readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                width: ListView.view ? ListView.view.width : 600
                height: 44
                radius: 10
                color: isCurrent
                    ? DesignTokens.surfaceSelected
                    : (dtMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                border.width: isCurrent ? 1 : 0
                border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                Behavior on color { ColorAnimation { duration: 110 } }

                Rectangle {
                    visible: dtRow.isCurrent
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
                        Layout.preferredWidth: 28
                        implicitHeight: 20

                        NowPlayingBars {
                            anchors.centerIn: parent
                            visible: dtRow.isCurrent
                            playing: root.controller.isPlaying
                            size: 13
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !dtRow.isCurrent
                            text: dtRow.trackNumber > 0 ? String(dtRow.trackNumber) : String(dtRow.index + 1)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            font.features: ({ "tnum": 1 })
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: dtRow.title
                        elide: Text.ElideRight
                        color: dtRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: dtRow.isCurrent ? Font.DemiBold : Font.Medium
                    }

                    FormatBadge { compact: true; text: dtRow.formatBadge }

                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: root.controller.formatTime(dtRow.durationMs)
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        font.features: ({ "tnum": 1 })
                    }
                }

                MouseArea {
                    id: dtMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.lib.playFromDetailModel(dtRow.index)
                }
            }
        }
    }
}
