import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    property string searchText: ""
    property bool compact: false
    property bool isMaximized: false
    property bool miniModeAvailable: true

    signal searchEdited(string text)
    signal toggleMiniMode()
    signal toggleTheme()
    signal openFilesRequested()
    signal minimizeClicked()
    signal maximizeRestoreClicked()
    signal closeClicked()

    implicitHeight: DesignTokens.titleBarHeight
    color: "transparent"

    function syncInteractiveRegions() {
        if (!root.Window.window || typeof platformBridge === "undefined" || !platformBridge) return
        const rects = []
        if (searchPill.width > 0 && searchPill.height > 0) {
            const sp = searchPill.mapToItem(null, 0, 0)
            rects.push({ x: sp.x, y: sp.y, width: searchPill.width, height: searchPill.height })
        }
        if (rightControlsRow.width > 0 && rightControlsRow.height > 0) {
            const rc = rightControlsRow.mapToItem(null, 0, 0)
            rects.push({ x: rc.x, y: rc.y, width: rightControlsRow.width, height: rightControlsRow.height })
        }
        platformBridge.setTitleBarInteractiveRects(rects)
    }

    onWidthChanged: syncInteractiveRegions()
    onHeightChanged: syncInteractiveRegions()
    Component.onCompleted: Qt.callLater(syncInteractiveRegions)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: DesignTokens.borderSubtle
    }

    // Cross-platform fallback for non-Win32 platforms (on Windows, Win32WindowHelper maps
    // empty titlebar space directly to HTCAPTION for native OS dragging & Aero Snap).
    Item {
        anchors.fill: parent
        visible: Qt.platform.os !== "windows"

        DragHandler {
            target: null
            dragThreshold: 4
            onActiveChanged: {
                if (active && root.Window.window) {
                    root.Window.window.startSystemMove()
                }
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: root.maximizeRestoreClicked()
        }
    }

    component BarActionBtn: Rectangle {
        id: btn
        property string iconName: "plus"
        property real iconSize: 14
        property bool isClose: false
        property real iconRotation: 0
        signal clicked()

        implicitWidth: isClose ? 30 : 28
        implicitHeight: 28
        radius: isClose ? 8 : 8
        color: {
            if (isClose) {
                if (btnMa.pressed) return "#c22b3d"
                if (btnMa.containsMouse) return "#e23d50"
                return "transparent"
            }
            if (btnMa.pressed) return DesignTokens.surfacePressed
            if (btnMa.containsMouse) return DesignTokens.surfaceHover
            return "transparent"
        }
        Behavior on color { ColorAnimation { duration: 130 } }

        scale: btnMa.pressed && !isClose ? 0.92 : 1.0
        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

        IconGlyph {
            anchors.centerIn: parent
            name: btn.iconName
            size: btn.iconSize
            rotation: btn.iconRotation
            color: (btn.isClose && btnMa.containsMouse)
                ? "#ffffff"
                : (btnMa.containsMouse ? DesignTokens.textPrimary : DesignTokens.textSecondary)
        }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: btn.isClose ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 10
        spacing: 12

        // Spatial brand mark
        RowLayout {
            spacing: 9
            Layout.fillWidth: false
            Layout.preferredWidth: root.compact ? DesignTokens.compactSidebarWidth : 205

            Rectangle {
                implicitWidth: 26
                implicitHeight: 26
                radius: DesignTokens.radiusSm
                color: "#f9d4df"

                IconGlyph {
                    anchors.centerIn: parent
                    name: "wave"
                    size: 17
                    color: "#d41842"
                }
            }

            Text {
                text: "Sonora"
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 18
                font.weight: Font.Bold
                font.letterSpacing: 0.25
            }

            Item { Layout.fillWidth: true }
        }

        // Center frosted search capsule (expands smoothly on focus)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: searchPill
                anchors.centerIn: parent
                readonly property real maxAllowed: Math.max(180, parent.width - 24)
                readonly property bool hovered: searchHover.hovered
                readonly property bool editing: searchInput.activeFocus
                width: Math.min(maxAllowed, editing ? 430 : (root.compact ? 252 : 332))
                height: 32
                radius: 16
                color: {
                    if (editing) return DesignTokens.isDark ? DesignTokens.surfaceCardHover : "#ffffff"
                    if (!hovered) return DesignTokens.surfaceInput
                    return DesignTokens.isDark ? DesignTokens.surfaceCard
                        : Qt.tint("#ffffff", Qt.alpha(DesignTokens.accent, 0.055))
                }
                border.width: 1
                border.color: editing ? DesignTokens.accent
                    : (hovered ? Qt.alpha(DesignTokens.accent, 0.48) : DesignTokens.borderOuter)

                Behavior on width {
                    NumberAnimation { duration: DesignTokens.animNormal; easing.type: Easing.OutQuint }
                }
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                onXChanged: root.syncInteractiveRegions()
                onYChanged: root.syncInteractiveRegions()
                onWidthChanged: root.syncInteractiveRegions()

                HoverHandler { id: searchHover }

                TapHandler {
                    onTapped: searchInput.forceActiveFocus(Qt.MouseFocusReason)
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: searchPill.radius + 2
                    color: "transparent"
                    border.color: DesignTokens.accentSoft
                    border.width: 2
                    opacity: searchPill.editing ? 1.0 : (searchPill.hovered ? 0.55 : 0.0)
                    Behavior on opacity { NumberAnimation { duration: DesignTokens.animFast } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    IconGlyph {
                        name: "search"
                        size: 14
                        color: searchPill.editing || (searchPill.hovered && !DesignTokens.isDark)
                            ? DesignTokens.accent
                            : (searchPill.hovered ? DesignTokens.textPrimary : DesignTokens.textTertiary)
                        Behavior on color { ColorAnimation { duration: DesignTokens.animFast } }
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        text: root.searchText
                        color: DesignTokens.textPrimary
                        selectionColor: DesignTokens.accent
                        selectedTextColor: DesignTokens.playbackInk
                        selectByMouse: true
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        clip: true
                        verticalAlignment: TextInput.AlignVCenter
                        onTextEdited: root.searchEdited(text)

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Search tracks, artists, albums...")
                            color: searchPill.editing || searchPill.hovered
                                ? DesignTokens.textSecondary : DesignTokens.textTertiary
                            font: searchInput.font
                            Behavior on color { ColorAnimation { duration: DesignTokens.animFast } }
                            visible: searchInput.text.length === 0 && !searchInput.inputMethodComposing
                        }
                    }

                    Rectangle {
                        readonly property bool shown: searchInput.text.length > 0
                        visible: opacity > 0.01
                        opacity: shown ? 1.0 : 0.0
                        scale: shown ? 1.0 : 0.6
                        implicitWidth: 18
                        implicitHeight: 18
                        radius: 9
                        color: clearMa.pressed ? DesignTokens.surfaceSelected
                            : (clearMa.containsMouse ? DesignTokens.accentSoft : "transparent")

                        Behavior on color { ColorAnimation { duration: DesignTokens.animFast } }
                        Behavior on opacity { NumberAnimation { duration: 130 } }
                        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }

                        IconGlyph {
                            anchors.centerIn: parent
                            name: "close"
                            size: 11
                            color: clearMa.containsMouse ? DesignTokens.accent : DesignTokens.textSecondary
                            Behavior on color { ColorAnimation { duration: DesignTokens.animFast } }
                        }

                        MouseArea {
                            id: clearMa
                            anchors.fill: parent
                            enabled: parent.shown
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchInput.clear()
                                root.searchEdited("")
                                searchInput.forceActiveFocus(Qt.MouseFocusReason)
                            }
                        }
                    }
                }
            }
        }

        // Utility buttons + Win11 caption controls
        RowLayout {
            id: rightControlsRow
            Layout.fillWidth: false
            spacing: 3
            onXChanged: root.syncInteractiveRegions()
            onYChanged: root.syncInteractiveRegions()
            onWidthChanged: root.syncInteractiveRegions()

            BarActionBtn {
                iconName: "plus"
                iconSize: 15
                onClicked: root.openFilesRequested()
            }

            BarActionBtn {
                id: themeBtn
                iconName: DesignTokens.isDark ? "sun" : "moon"
                iconSize: 15
                property real spinAngle: 0
                iconRotation: spinAngle
                NumberAnimation {
                    id: themeSpinAnim
                    target: themeBtn
                    property: "spinAngle"
                    from: 0
                    to: 360
                    duration: 380
                    easing.type: Easing.OutQuint
                }
                onClicked: {
                    themeSpinAnim.restart()
                    root.toggleTheme()
                }
            }

            BarActionBtn {
                visible: root.miniModeAvailable
                iconName: "mini"
                iconSize: 15
                onClicked: root.toggleMiniMode()
            }

            Rectangle {
                implicitWidth: 1
                implicitHeight: 14
                color: DesignTokens.borderOuter
                Layout.leftMargin: 5
                Layout.rightMargin: 5
            }

            BarActionBtn {
                implicitWidth: 34
                iconName: "minimize"
                iconSize: 13
                onClicked: root.minimizeClicked()
            }

            BarActionBtn {
                implicitWidth: 34
                iconName: root.isMaximized ? "restore" : "maximize"
                iconSize: 12
                onClicked: root.maximizeRestoreClicked()
            }

            BarActionBtn {
                isClose: true
                iconName: "close"
                iconSize: 13
                onClicked: root.closeClicked()
            }
        }
    }
}
