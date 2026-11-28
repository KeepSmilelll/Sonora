import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var onlineCtrl
    required property var lib
    required property var settingsObj

    signal openSettingsRequested()
    signal inspectTrack(var trackMap)

    readonly property bool compact: width < 660
    readonly property var tasks: root.onlineCtrl.downloadTasks

    function statusText(st) {
        if (st === "queued") return qsTr("Queued")
        if (st === "downloading") return qsTr("Downloading...")
        if (st === "done") return qsTr("Saved")
        if (st === "error") return qsTr("Failed")
        return ""
    }

    function statusColor(st) {
        if (st === "done") return DesignTokens.badgeTeal
        if (st === "error") return "#e24d5c"
        return DesignTokens.accentLight
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.width < 680 ? 16 : (root.width >= 1560 ? 26 : 22)
        spacing: 14

        // Header Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.minimumWidth: 0
                elide: Text.ElideRight
                text: qsTr("Downloads & Cache")
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
                visible: !root.compact && root.onlineCtrl.cacheUsageText.length > 0
                text: root.onlineCtrl.cacheUsageText
            }

            Item { Layout.fillWidth: true }

            VirelaButton {
                compact: true
                iconName: "trash"
                text: qsTr("Clear Cache")
                onClicked: root.onlineCtrl.clearStreamCache()
            }

            VirelaButton {
                compact: true
                iconName: "settings"
                text: qsTr("Settings")
                onClicked: root.openSettingsRequested()
            }
        }

        // Download Folder Strip
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            radius: 10
            color: DesignTokens.surfaceInput
            border.width: 1
            border.color: DesignTokens.hairlineBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                IconGlyph { name: "folder"; size: 14; color: DesignTokens.accent }
                Text {
                    text: qsTr("Download Folder:")
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
                Text {
                    Layout.fillWidth: true
                    text: root.settingsObj.onlineDownloadDir
                    elide: Text.ElideMiddle
                    color: DesignTokens.textSecondary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 12
                }
            }
        }

        // Active & Recent Download Queue Section
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.tasks.length > 0
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Download Queue")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                VirelaButton {
                    compact: true
                    flat: true
                    text: qsTr("Clear Finished")
                    onClicked: root.onlineCtrl.clearFinishedDownloads()
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(210, root.tasks.length * 52)
                clip: true
                spacing: 3
                boundsBehavior: Flickable.StopAtBounds
                model: root.tasks
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: taskRow
                    required property var modelData
                    width: ListView.view ? ListView.view.width : 500
                    height: 49
                    radius: DesignTokens.radiusSm
                    color: taskMa.containsMouse ? DesignTokens.surfaceHover : DesignTokens.stageSurfaceBg
                    border.width: 1
                    border.color: DesignTokens.hairlineBorder

                    MouseArea {
                        id: taskMa
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            radius: 7
                            color: DesignTokens.surfaceInput
                            clip: true

                            Image {
                                id: tImg
                                anchors.fill: parent
                                source: taskRow.modelData.coverUrl || ""
                                sourceSize: Qt.size(64, 64)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: status === Image.Ready
                            }
                            IconGlyph {
                                anchors.centerIn: parent
                                visible: tImg.status !== Image.Ready
                                name: "download"
                                size: 14
                                color: DesignTokens.textTertiary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: taskRow.modelData.title || ""
                                elide: Text.ElideRight
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Text {
                                Layout.fillWidth: true
                                text: taskRow.modelData.status === "error" && taskRow.modelData.error
                                    ? taskRow.modelData.error
                                    : (taskRow.modelData.artist || qsTr("Unknown Artist"))
                                elide: Text.ElideRight
                                color: taskRow.modelData.status === "error" ? "#e24d5c" : DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        FormatBadge {
                            compact: true
                            visible: Boolean(taskRow.modelData.formatBadge)
                            text: taskRow.modelData.formatBadge || ""
                        }

                        Text {
                            text: root.statusText(taskRow.modelData.status || "")
                            color: root.statusColor(taskRow.modelData.status || "")
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }

                        VirelaButton {
                            visible: taskRow.modelData.status === "error"
                            compact: true
                            iconName: "refresh"
                            text: qsTr("Retry")
                            onClicked: root.onlineCtrl.retryDownload(taskRow.modelData.trackKey)
                        }

                        VirelaButton {
                            visible: taskRow.modelData.status === "done"
                            compact: true
                            iconName: "play"
                            text: qsTr("Play")
                            onClicked: root.onlineCtrl.playDownloadTask(taskRow.modelData.trackKey)
                        }
                    }
                }
            }
        }

        // Downloaded Library Tracks Section
        RowLayout {
            Layout.fillWidth: true
            visible: root.lib.downloadedTracksModel.count > 0 || root.tasks.length > 0
            spacing: 8

            Text {
                text: qsTr("Downloaded to Library")
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 15
                font.weight: Font.Bold
            }

            Rectangle {
                implicitWidth: dlCntTxt.implicitWidth + 14
                implicitHeight: 22
                radius: 11
                color: DesignTokens.surfaceInput
                border.width: 1
                border.color: DesignTokens.hairlineBorder
                Text {
                    id: dlCntTxt
                    anchors.centerIn: parent
                    text: String(root.lib.downloadedTracksModel.count)
                    color: DesignTokens.textTertiary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 11
                    font.features: ({ "tnum": 1 })
                }
            }

            Item { Layout.fillWidth: true }

            VirelaButton {
                visible: root.lib.downloadedTracksModel.count > 0
                compact: true
                primary: true
                iconName: "play"
                text: qsTr("Play All")
                onClicked: root.lib.playFromDownloadedModel(0)
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: dlList
                anchors.fill: parent
                visible: root.lib.downloadedTracksModel.count > 0
                clip: true
                spacing: 2
                reuseItems: true
                cacheBuffer: 160
                boundsBehavior: Flickable.StopAtBounds
                model: root.lib.downloadedTracksModel
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: dlRow
                    required property int index
                    required property int trackId
                    required property string title
                    required property string artist
                    required property string album
                    required property int durationMs
                    required property string formatBadge
                    required property string coverHash

                    readonly property bool isCurrent: root.controller.trackId >= 0 && root.controller.trackId === trackId
                    readonly property bool hovered: dlMa.containsMouse || dlThumbMa.containsMouse || dlInfoMa.containsMouse
                    width: dlList.width
                    height: 46
                    radius: DesignTokens.radiusSm
                    color: isCurrent
                        ? DesignTokens.surfaceSelected
                        : (hovered ? DesignTokens.surfaceHover : "transparent")
                    border.width: isCurrent ? 1 : 0
                    border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.28)
                    Behavior on color { ColorAnimation { duration: 110 } }

                    Rectangle {
                        visible: dlRow.isCurrent
                        width: 3
                        height: 18
                        radius: 1.5
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: DesignTokens.accent
                    }

                    MouseArea {
                        id: dlMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onDoubleClicked: root.lib.playFromDownloadedModel(dlRow.index)
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
                                visible: dlRow.isCurrent
                                controller: root.controller
                                size: 13
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !dlRow.isCurrent
                                text: String(dlRow.index + 1).padStart(2, "0")
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                                font.features: ({ "tnum": 1 })
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            radius: 7
                            color: DesignTokens.surfaceCard
                            border.width: 1
                            border.color: dlRow.isCurrent ? DesignTokens.accent : DesignTokens.borderSubtle
                            clip: true

                            Image {
                                id: dlThumb
                                anchors.fill: parent
                                source: dlRow.isCurrent ? root.controller.trackCoverUrl
                                    : (dlRow.coverHash.length > 0 ? ("image://cover/" + dlRow.coverHash + "?size=96") : "")
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: status === Image.Ready
                            }
                            IconGlyph {
                                anchors.centerIn: parent
                                visible: dlThumb.status !== Image.Ready && !dlThumbMa.containsMouse
                                name: "music"
                                size: 14
                                color: DesignTokens.textTertiary
                            }
                            Rectangle {
                                anchors.fill: parent
                                visible: dlThumbMa.containsMouse
                                color: dlThumb.visible ? Qt.rgba(0, 0, 0, 0.46) : "transparent"
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: dlRow.isCurrent && root.controller.isPlaying ? "pause" : "play"
                                    size: 13
                                    color: dlThumb.visible ? "#ffffff" : DesignTokens.accent
                                }
                            }
                            MouseArea {
                                id: dlThumbMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (dlRow.isCurrent) root.controller.togglePlayPause()
                                    else root.lib.playFromDownloadedModel(dlRow.index)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: dlRow.title
                                elide: Text.ElideRight
                                color: dlRow.isCurrent ? DesignTokens.accentLight : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: dlRow.isCurrent ? Font.DemiBold : Font.Medium
                            }
                            Text {
                                Layout.fillWidth: true
                                text: dlRow.album.length > 0
                                    ? ((dlRow.artist || qsTr("Unknown Artist")) + " · " + dlRow.album)
                                    : (dlRow.artist || qsTr("Unknown Artist"))
                                elide: Text.ElideRight
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }

                        FormatBadge {
                            compact: true
                            visible: dlRow.formatBadge.length > 0
                            text: dlRow.formatBadge
                        }

                        Text {
                            Layout.preferredWidth: 44
                            horizontalAlignment: Text.AlignRight
                            text: root.controller.formatTime(dlRow.durationMs)
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.features: ({ "tnum": 1 })
                        }

                        Rectangle {
                            implicitWidth: 26
                            implicitHeight: 26
                            radius: 8
                            opacity: dlRow.hovered ? 1.0 : 0.45
                            color: dlInfoMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            IconGlyph {
                                anchors.centerIn: parent
                                name: "info"
                                size: 13
                                color: dlInfoMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textTertiary
                            }
                            MouseArea {
                                id: dlInfoMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.inspectTrack(root.lib.downloadedTracksModel.trackMapAt(dlRow.index))
                            }
                        }
                    }
                }
            }

            // Empty State Card
            GlassPanel {
                anchors.centerIn: parent
                visible: root.lib.downloadedTracksModel.count === 0 && root.tasks.length === 0
                width: Math.min(parent.width - 48, 440)
                height: emptyDlCol.implicitHeight + 48
                radius: DesignTokens.radiusXl
                panelColor: DesignTokens.stageSurfaceBg
                borderColor: DesignTokens.hairlineBorder
                shadowEnabled: false

                ColumnLayout {
                    id: emptyDlCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 10

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 56
                        implicitHeight: 56
                        radius: 17
                        color: DesignTokens.accentSoft
                        IconGlyph { anchors.centerIn: parent; name: "download"; size: 24; color: DesignTokens.accent }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("No Downloaded Tracks Yet")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Tracks you download from online music will appear here with embedded artwork, lyrics, and automatic local library indexing.")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 12
                        lineHeight: 1.45
                    }
                }
            }
        }
    }
}
