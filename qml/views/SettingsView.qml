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

    component SectionCard: Rectangle {
        default property alias content: inner.data
        property string title: ""
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 36
        radius: DesignTokens.radiusLg
        color: DesignTokens.surfaceCard
        border.width: 1
        border.color: DesignTokens.borderSubtle

        ColumnLayout {
            id: inner
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: 18
            spacing: 12

            Text {
                text: title
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 15
                font.weight: Font.Bold
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: mainCol.implicitHeight + 48
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: mainCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }
                FormatBadge {
                    text: root.controller.memoryUsageText
                }
            }

            // 1. Audio Output & WASAPI Audiophile Settings
            SectionCard {
                title: qsTr("Audio Engine & WASAPI Output")

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Output Device")
                        color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13
                    }
                    ComboBox {
                        Layout.preferredWidth: 260
                        model: root.controller.audioDevices
                        textRole: "name"
                        onActivated: (idx) => {
                            const d = root.controller.audioDevices[idx]
                            if (d) root.settingsObj.outputDeviceId = d.id
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text { text: qsTr("WASAPI Exclusive Mode"); color: DesignTokens.textPrimary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                        Text { text: qsTr("Bypass Windows mixer for bit-perfect playback and automatic hardware sample rate switching."); color: DesignTokens.textTertiary; font.family: DesignTokens.fontFamily; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    Switch {
                        checked: root.settingsObj.wasapiExclusive
                        onToggled: root.settingsObj.wasapiExclusive = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Auto Native Sample Rate Switching"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    Switch {
                        checked: root.settingsObj.autoNativeSampleRate
                        enabled: root.settingsObj.wasapiExclusive
                        onToggled: root.settingsObj.autoNativeSampleRate = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Bit-Perfect Bypass (skip DSP when EQ/RG off & volume 100%)"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    Switch {
                        checked: root.settingsObj.bitPerfectBypass
                        onToggled: root.settingsObj.bitPerfectBypass = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("DSD to PCM Target Sample Rate"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: ["88.2 kHz", "176.4 kHz", "352.8 kHz"]
                        currentIndex: root.settingsObj.dsdTargetSampleRate === 88200 ? 0 : (root.settingsObj.dsdTargetSampleRate === 352800 ? 2 : 1)
                        onActivated: (idx) => {
                            root.settingsObj.dsdTargetSampleRate = idx === 0 ? 88200 : (idx === 2 ? 352800 : 176400)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("ReplayGain Loudness Normalization"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: [qsTr("Off"), qsTr("Track Gain"), qsTr("Album Gain")]
                        currentIndex: root.settingsObj.replayGainMode
                        onActivated: (idx) => root.settingsObj.replayGainMode = idx
                    }
                }
            }

            // 2. Appearance, Material & Language
            SectionCard {
                title: qsTr("Appearance & Language")

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Theme Mode"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: [qsTr("Follow System"), qsTr("Dark"), qsTr("Light")]
                        currentIndex: root.themeObj.themeMode === "dark" ? 1 : (root.themeObj.themeMode === "light" ? 2 : 0)
                        onActivated: (idx) => {
                            root.themeObj.themeMode = idx === 1 ? "dark" : (idx === 2 ? "light" : "system")
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Window Backdrop Material"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    ComboBox {
                        Layout.preferredWidth: 180
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
                    Text { Layout.fillWidth: true; text: qsTr("Interface Language"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: ["简体中文", "English"]
                        currentIndex: root.themeObj.language === "en_US" ? 1 : 0
                        onActivated: (idx) => {
                            root.themeObj.language = idx === 1 ? "en_US" : "zh_CN"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Real-Time Spectrum Visualizer"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    Switch {
                        checked: root.settingsObj.visualizerEnabled
                        onToggled: root.settingsObj.visualizerEnabled = checked
                    }
                }
            }

            // 3. Music Library Folders
            SectionCard {
                title: qsTr("Watched Music Folders")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        implicitHeight: 30; implicitWidth: addL.implicitWidth + 22; radius: 15
                        color: DesignTokens.accent
                        Text { id: addL; anchors.centerIn: parent; text: qsTr("+ Add Folder"); color: "#ffffff"; font.family: DesignTokens.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.addFolderRequested() }
                    }
                    Rectangle {
                        implicitHeight: 30; implicitWidth: scanL.implicitWidth + 22; radius: 15
                        color: DesignTokens.surfaceHover
                        border.width: 1; border.color: DesignTokens.borderSubtle
                        Text { id: scanL; anchors.centerIn: parent; text: root.lib.scanning ? qsTr("Scanning...") : qsTr("Rescan Now"); color: DesignTokens.textPrimary; font.family: DesignTokens.fontFamily; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.lib.rescanLibrary() }
                    }
                }

                Repeater {
                    model: root.settingsObj.libraryFolders
                    delegate: RowLayout {
                        id: fldRow
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 8
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
                            implicitWidth: 26; implicitHeight: 26; radius: 6
                            color: rmFMa.containsMouse ? DesignTokens.surfacePressed : "transparent"
                            IconGlyph { anchors.centerIn: parent; name: "trash"; size: 13; color: DesignTokens.textTertiary }
                            MouseArea {
                                id: rmFMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: root.lib.removeWatchedFolder(fldRow.modelData)
                            }
                        }
                    }
                }
            }

            // 4. Desktop Integration
            SectionCard {
                title: qsTr("Desktop Integration (Windows)")

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: qsTr("Minimize to System Tray on Close"); color: DesignTokens.textSecondary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                    Switch {
                        checked: root.settingsObj.closeToTray
                        onToggled: root.settingsObj.closeToTray = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text { text: qsTr("Global Media Hotkeys"); color: DesignTokens.textPrimary; font.family: DesignTokens.fontFamily; font.pixelSize: 13 }
                        Text { text: "Ctrl+Alt+Space (Play/Pause) · Ctrl+Alt+Left/Right (Prev/Next) · Ctrl+Alt+Up/Down (Volume) · Ctrl+Alt+M (Mini Mode)"; color: DesignTokens.textTertiary; font.family: DesignTokens.fontFamily; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    Switch {
                        checked: root.settingsObj.globalHotkeysEnabled
                        onToggled: root.settingsObj.globalHotkeysEnabled = checked
                    }
                }
            }
        }
    }
}
