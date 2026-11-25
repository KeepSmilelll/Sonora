import QtQuick
import QtQuick.Layouts
import Sonora.UI

Rectangle {
    id: root

    property string searchText: ""
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

    // Drag window from empty titlebar areas
    DragHandler {
        target: null
        onActiveChanged: {
            if (active && root.Window.window) {
                root.Window.window.startSystemMove()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onDoubleClicked: root.maximizeRestoreClicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 12

        // App brand mark
        RowLayout {
            spacing: 8
            Layout.preferredWidth: DesignTokens.sidebarWidth - 28

            Rectangle {
                implicitWidth: 24
                implicitHeight: 24
                radius: 7
                gradient: Gradient {
                    GradientStop { position: 0.0; color: DesignTokens.accent }
                    GradientStop { position: 1.0; color: Qt.darker(DesignTokens.accent, 1.35) }
                }

                IconGlyph {
                    anchors.centerIn: parent
                    name: "music"
                    size: 13
                    color: "#ffffff"
                    strokeWidth: 2.1
                }
            }

            Text {
                text: "Sonora"
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
            }
        }

        // Center frosted search pill
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: searchPill
                anchors.centerIn: parent
                width: Math.min(parent.width - 20, 380)
                height: 30
                radius: 15
                color: DesignTokens.surfaceCard
                border.width: 1
                border.color: searchInput.activeFocus ? DesignTokens.accent : DesignTokens.borderSubtle

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 10
                    spacing: 7

                    IconGlyph {
                        name: "search"
                        size: 14
                        color: searchInput.activeFocus ? DesignTokens.accent : DesignTokens.textTertiary
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        text: root.searchText
                        color: DesignTokens.textPrimary
                        selectionColor: DesignTokens.accent
                        selectedTextColor: "#ffffff"
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        clip: true
                        verticalAlignment: TextInput.AlignVCenter
                        onTextEdited: root.searchEdited(text)

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Search tracks, artists, albums...")
                            color: DesignTokens.textTertiary
                            font: searchInput.font
                            visible: searchInput.text.length === 0
                        }
                    }

                    MouseArea {
                        visible: searchInput.text.length > 0
                        implicitWidth: 18
                        implicitHeight: 18
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = ""
                            root.searchEdited("")
                        }
                        IconGlyph {
                            anchors.centerIn: parent
                            name: "close"
                            size: 12
                            color: DesignTokens.textSecondary
                        }
                    }
                }
            }
        }

        // Utility buttons + Win11 caption controls
        RowLayout {
            spacing: 4

            // Quick open audio files
            Rectangle {
                implicitWidth: 30; implicitHeight: 28; radius: 7
                color: openMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    name: "plus"
                    size: 15
                    color: DesignTokens.textSecondary
                }
                MouseArea {
                    id: openMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openFilesRequested()
                }
            }

            // Theme toggle
            Rectangle {
                implicitWidth: 30; implicitHeight: 28; radius: 7
                color: themeMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    name: DesignTokens.isDark ? "sun" : "moon"
                    size: 15
                    color: DesignTokens.textSecondary
                }
                MouseArea {
                    id: themeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleTheme()
                }
            }

            // Mini mode toggle
            Rectangle {
                visible: root.miniModeAvailable
                implicitWidth: 30; implicitHeight: 28; radius: 7
                color: miniMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    name: "mini"
                    size: 15
                    color: DesignTokens.textSecondary
                }
                MouseArea {
                    id: miniMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleMiniMode()
                }
            }

            Rectangle {
                implicitWidth: 1; implicitHeight: 14
                color: DesignTokens.borderSubtle
                Layout.leftMargin: 4
                Layout.rightMargin: 4
            }

            // Minimize
            Rectangle {
                implicitWidth: 34; implicitHeight: 28; radius: 6
                color: minMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "minimize"; size: 13; color: DesignTokens.textSecondary }
                MouseArea {
                    id: minMa; anchors.fill: parent; hoverEnabled: true
                    onClicked: root.minimizeClicked()
                }
            }

            // Maximize / Restore
            Rectangle {
                implicitWidth: 34; implicitHeight: 28; radius: 6
                color: maxMa.containsMouse ? DesignTokens.surfaceHover : "transparent"
                IconGlyph { anchors.centerIn: parent; name: "maximize"; size: 12; color: DesignTokens.textSecondary }
                MouseArea {
                    id: maxMa; anchors.fill: parent; hoverEnabled: true
                    onClicked: root.maximizeRestoreClicked()
                }
            }

            // Close
            Rectangle {
                implicitWidth: 34; implicitHeight: 28; radius: 6
                color: closeMa.containsMouse ? "#d93848" : "transparent"
                IconGlyph {
                    anchors.centerIn: parent
                    name: "close"
                    size: 13
                    color: closeMa.containsMouse ? "#ffffff" : DesignTokens.textSecondary
                }
                MouseArea {
                    id: closeMa; anchors.fill: parent; hoverEnabled: true
                    onClicked: root.closeClicked()
                }
            }
        }
    }

    // Subtle bottom hairline
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: DesignTokens.borderSubtle
    }
}
