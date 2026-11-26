import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Sonora.UI

Item {
    id: root

    required property var controller
    required property var lib
    required property var settingsObj
    required property var themeObj

    signal addFolderRequested()

    Component.onCompleted: {
        root.controller.refreshAudioDevices()
        root.controller.refreshMemoryStats()
    }

    Timer {
        interval: 2000
        repeat: true
        running: root.visible
        onTriggered: root.controller.refreshMemoryStats()
    }

    component SectionCard: GlassPanel {
        id: cardRoot
        default property alias content: inner.data
        property string title: ""
        property string iconName: "settings"

        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 38
        radius: DesignTokens.radiusLg
        shadowEnabled: false

        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 19
            spacing: 13

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    implicitWidth: 30
                    implicitHeight: 30
                    radius: 9
                    color: DesignTokens.accentSoft
                    IconGlyph {
                        anchors.centerIn: parent
                        name: cardRoot.iconName
                        size: 15
                        color: DesignTokens.accent
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: cardRoot.title
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: DesignTokens.borderSubtle
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: mainCol.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: mainCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 13

                Rectangle {
                    implicitWidth: 42
                    implicitHeight: 42
                    radius: 13
                    color: DesignTokens.accentSoft
                    border.width: 1
                    border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.25)
                    IconGlyph { anchors.centerIn: parent; name: "settings"; size: 19; color: DesignTokens.accent }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 23
                    font.weight: Font.Bold
                }

                FormatBadge {
                    text: root.controller.memoryUsageText
                }
            }

            // 1. Audio Output & WASAPI Audiophile Settings
            SectionCard {
                iconName: "wave"
                title: qsTr("Audio Engine & WASAPI Output")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Output Device")
                        color: DesignTokens.textPrimary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 268
                        model: root.controller.audioDevices
                        textRole: "name"
                        currentIndex: {
                            const list = root.controller.audioDevices
                            if (!list || list.length === undefined) return 0
                            for (let i = 0; i < list.length; ++i) {
                                if (list[i] && list[i].id === root.settingsObj.outputDeviceId) return i
                            }
                            return 0
                        }
                        onActivated: (idx) => {
                            const d = root.controller.audioDevices[idx]
                            if (d) root.settingsObj.outputDeviceId = d.id
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("WASAPI Exclusive Mode")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Bypass Windows mixer for bit-perfect playback and automatic hardware sample rate switching.")
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.wasapiExclusive
                        onToggled: (state) => root.settingsObj.wasapiExclusive = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Auto Native Sample Rate Switching")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.autoNativeSampleRate
                        enabled: root.settingsObj.wasapiExclusive
                        onToggled: (state) => root.settingsObj.autoNativeSampleRate = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Bit-Perfect Bypass (skip DSP when EQ/RG off & volume 100%)")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.bitPerfectBypass
                        onToggled: (state) => root.settingsObj.bitPerfectBypass = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("DSD to PCM Target Sample Rate")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 168
                        model: ["88.2 kHz", "176.4 kHz", "352.8 kHz"]
                        currentIndex: root.settingsObj.dsdTargetSampleRate === 88200 ? 0 : (root.settingsObj.dsdTargetSampleRate === 352800 ? 2 : 1)
                        onActivated: (idx) => {
                            root.settingsObj.dsdTargetSampleRate = idx === 0 ? 88200 : (idx === 2 ? 352800 : 176400)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("ReplayGain Loudness Normalization")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 168
                        model: [qsTr("Off"), qsTr("Track Gain"), qsTr("Album Gain")]
                        currentIndex: root.settingsObj.replayGainMode
                        onActivated: (idx) => root.settingsObj.replayGainMode = idx
                    }
                }
            }

            // 2. Appearance, Material & Language
            SectionCard {
                iconName: "sun"
                title: qsTr("Appearance & Language")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Theme Mode")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 168
                        model: [qsTr("Follow System"), qsTr("Dark"), qsTr("Light")]
                        currentIndex: root.themeObj.themeMode === "dark" ? 1 : (root.themeObj.themeMode === "light" ? 2 : 0)
                        onActivated: (idx) => {
                            root.themeObj.themeMode = idx === 1 ? "dark" : (idx === 2 ? "light" : "system")
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Window Backdrop Material")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 192
                        model: [qsTr("Windows 11 Mica"), qsTr("Windows 11 Acrylic"), qsTr("Ambient Cover Glow"), qsTr("Solid Minimal")]
                        currentIndex: {
                            if (root.themeObj.backdropMode === "mica") return 0
                            if (root.themeObj.backdropMode === "acrylic") return 1
                            if (root.themeObj.backdropMode === "ambient") return 2
                            return 3
                        }
                        onActivated: (idx) => {
                            const modes = ["mica", "acrylic", "ambient", "solid"]
                            root.themeObj.backdropMode = modes[idx]
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Interface Language")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraComboBox {
                        Layout.preferredWidth: 168
                        model: ["简体中文", "English"]
                        currentIndex: root.themeObj.language === "en_US" ? 1 : 0
                        onActivated: (idx) => {
                            root.themeObj.language = idx === 1 ? "en_US" : "zh_CN"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Real-Time Spectrum Visualizer")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.visualizerEnabled
                        onToggled: (state) => root.settingsObj.visualizerEnabled = state
                    }
                }
            }

            // 3. Music Library Folders
            SectionCard {
                iconName: "folder"
                title: qsTr("Watched Music Folders")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    SonoraButton {
                        primary: true
                        compact: true
                        iconName: "plus"
                        text: qsTr("Add Folder")
                        onClicked: root.addFolderRequested()
                    }

                    SonoraButton {
                        compact: true
                        enabled: !root.lib.scanning
                        text: root.lib.scanning ? qsTr("Scanning...") : qsTr("Rescan Now")
                        onClicked: root.lib.rescanLibrary()
                    }
                }

                Repeater {
                    model: root.settingsObj.libraryFolders
                    delegate: Rectangle {
                        id: fldRow
                        required property string modelData
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 10
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: DesignTokens.borderSubtle

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 9

                            IconGlyph { name: "folder"; size: 14; color: DesignTokens.accent }
                            Text {
                                Layout.fillWidth: true
                                text: fldRow.modelData
                                elide: Text.ElideMiddle
                                color: DesignTokens.textSecondary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                            }
                            Rectangle {
                                implicitWidth: 26
                                implicitHeight: 26
                                radius: 7
                                color: rmFMa.containsMouse ? Qt.rgba(0.88, 0.22, 0.30, 0.18) : "transparent"
                                IconGlyph {
                                    anchors.centerIn: parent
                                    name: "trash"
                                    size: 13
                                    color: rmFMa.containsMouse ? "#f25565" : DesignTokens.textTertiary
                                }
                                MouseArea {
                                    id: rmFMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.lib.removeWatchedFolder(fldRow.modelData)
                                }
                            }
                        }
                    }
                }
            }

            // 4. Desktop Integration
            SectionCard {
                iconName: "mini"
                title: qsTr("Desktop Integration (Windows)")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Minimize to System Tray on Close")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.closeToTray
                        onToggled: (state) => root.settingsObj.closeToTray = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Global Media Hotkeys")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Ctrl+Alt+Space (Play/Pause) · Ctrl+Alt+Left/Right (Prev/Next) · Ctrl+Alt+Up/Down (Volume) · Ctrl+Alt+M (Mini Mode)"
                            color: DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                        }
                    }
                    SonoraSwitch {
                        checked: root.settingsObj.globalHotkeysEnabled
                        onToggled: (state) => root.settingsObj.globalHotkeysEnabled = state
                    }
                }
            }
        }
    }
}
