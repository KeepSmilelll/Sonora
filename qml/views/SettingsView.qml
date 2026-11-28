import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    required property var controller
    required property var lib
    required property var settingsObj
    required property var themeObj
    required property var onlineCtrl

    signal addFolderRequested()
    signal selectDownloadDirRequested()
    signal selectNeteaseServiceDirRequested()
    signal selectMetingServiceDirRequested()
    signal themeModeChangeRequested(string mode, real wx, real wy)
    property string activeSettingsTab: "online" // "online" | "audio" | "local" | "appearance"
    readonly property int horizontalPadding: width < 680 ? 16 : 22
    readonly property real contentWidth: Math.min(width - horizontalPadding * 2, DesignTokens.settingsMaxWidth)

    function showOnlineSources() {
        activeSettingsTab = "online"
        settingsScroll.contentY = 0
    }

    Component.onCompleted: {
        root.controller.refreshAudioDevices()
        root.controller.refreshMemoryStats()
        root.onlineCtrl.refreshCacheStats()
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
        panelColor: DesignTokens.stageSurfaceBg
        borderColor: DesignTokens.hairlineBorder
        specularEnabled: true
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
                color: DesignTokens.hairlineBorder
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Pinned Settings Header + 4-Tab Segmented Navigation
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.contentWidth
            Layout.fillWidth: false
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.topMargin: 22
            Layout.bottomMargin: 12
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    color: DesignTokens.textPrimary
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    font.letterSpacing: -0.3
                }

                FormatBadge {
                    text: root.controller.memoryUsageText
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: segFlow.implicitHeight + 8
                radius: DesignTokens.radiusLg
                color: DesignTokens.stageSurfaceBg
                border.width: 1
                border.color: DesignTokens.hairlineBorder

                Flow {
                    id: segFlow
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    Repeater {
                        model: [
                            { key: "online",     icon: "cloud",    label: qsTr("Online & Cache") },
                            { key: "audio",      icon: "wave",     label: qsTr("Audio & Output") },
                            { key: "local",      icon: "folder",   label: qsTr("Local Library") },
                            { key: "appearance", icon: "sun",      label: qsTr("Appearance & Desktop") }
                        ]
                        delegate: Rectangle {
                            id: segTab
                            required property var modelData
                            readonly property bool active: root.activeSettingsTab === modelData.key
                            implicitWidth: segRow.implicitWidth + 24
                            implicitHeight: 32
                            radius: DesignTokens.radiusMd
                            color: active
                                ? DesignTokens.surfaceSelected
                                : (segMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                            border.width: active ? 1 : 0
                            border.color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.34)
                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                id: segRow
                                anchors.centerIn: parent
                                spacing: 7

                                IconGlyph {
                                    name: segTab.modelData.icon
                                    size: 13
                                    color: segTab.active ? DesignTokens.accent : DesignTokens.textSecondary
                                }
                                Text {
                                    text: segTab.modelData.label
                                    color: segTab.active ? DesignTokens.accentLight : DesignTokens.textSecondary
                                    font.family: DesignTokens.fontFamily
                                    font.pixelSize: 12
                                    font.weight: segTab.active ? Font.DemiBold : Font.Medium
                                }
                            }

                            MouseArea {
                                id: segMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.activeSettingsTab !== segTab.modelData.key) {
                                        root.activeSettingsTab = segTab.modelData.key
                                        settingsScroll.contentY = 0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Flickable {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: mainCol.implicitHeight + 36
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: mainCol
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.contentWidth
                anchors.top: parent.top
                anchors.topMargin: 2
                spacing: 16

                // 1. Audio Output & WASAPI Audiophile Settings
                SectionCard {
                    visible: root.activeSettingsTab === "audio"
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
                    VirelaComboBox {
                        Layout.preferredWidth: Math.min(268, Math.max(176, Math.round(root.contentWidth * 0.36)))
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
                    VirelaSwitch {
                        checked: root.settingsObj.wasapiExclusive
                        onToggled: (state) => root.settingsObj.wasapiExclusive = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Auto Native Sample Rate Switching")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaSwitch {
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
                        wrapMode: Text.WordWrap
                        text: qsTr("Bit-Perfect Bypass (skip DSP when EQ/RG off & volume 100%)")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaSwitch {
                        checked: root.settingsObj.bitPerfectBypass
                        onToggled: (state) => root.settingsObj.bitPerfectBypass = state
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("DSD to PCM Target Sample Rate")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
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
                        wrapMode: Text.WordWrap
                        text: qsTr("ReplayGain Loudness Normalization")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
                        Layout.preferredWidth: 168
                        model: [qsTr("Off"), qsTr("Track Gain"), qsTr("Album Gain")]
                        currentIndex: root.settingsObj.replayGainMode
                        onActivated: (idx) => root.settingsObj.replayGainMode = idx
                    }
                }
            }

            // 2. Appearance, Material & Language
            SectionCard {
                visible: root.activeSettingsTab === "appearance"
                iconName: "sun"
                title: qsTr("Appearance & Language")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Theme Mode")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
                        id: themeModeCombo
                        autoSelect: false
                        Layout.preferredWidth: 168
                        model: [qsTr("Follow System"), qsTr("Dark"), qsTr("Light")]
                        currentIndex: root.themeObj.themeMode === "dark" ? 1 : (root.themeObj.themeMode === "light" ? 2 : 0)
                        onActivated: (idx) => {
                            const targetMode = idx === 1 ? "dark" : (idx === 2 ? "light" : "system")
                            const p = themeModeCombo.mapToItem(null, themeModeCombo.width * 0.5, themeModeCombo.height * 0.5)
                            root.themeModeChangeRequested(targetMode, p.x, p.y)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Window Background Style")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
                        readonly property var modes: ["ambient", "solid"]
                        Layout.preferredWidth: 192
                        autoSelect: false
                        model: [qsTr("Ambient Cover Glow"), qsTr("Solid Minimal")]
                        currentIndex: modes.indexOf(root.themeObj.backdropMode)
                        onActivated: (idx) => root.themeObj.backdropMode = modes[idx]
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Interface Language")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
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
                        wrapMode: Text.WordWrap
                        text: qsTr("Real-Time Spectrum Visualizer")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaSwitch {
                        checked: root.settingsObj.visualizerEnabled
                        onToggled: (state) => root.settingsObj.visualizerEnabled = state
                    }
                }
            }

            // 3. Online Sources & Stream Cache
            SectionCard {
                id: onlineSourcesCard
                visible: root.activeSettingsTab === "online"
                iconName: "cloud"
                title: qsTr("Online Sources & Cache")

                // NeteaseCloudMusicApi URL
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Netease API URL (Primary Backend)")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        Text {
                            visible: root.onlineCtrl.neteaseTestStatus.length > 0
                            text: root.onlineCtrl.neteaseTestStatus
                            color: root.onlineCtrl.neteaseTestOk ? DesignTokens.badgeTeal : DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        radius: 10
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: neteaseUrlInput.activeFocus ? DesignTokens.accent : DesignTokens.borderOuter

                        TextInput {
                            id: neteaseUrlInput
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: TextInput.AlignVCenter
                            text: root.settingsObj.onlineNeteaseApiUrl
                            color: DesignTokens.textPrimary
                            selectionColor: DesignTokens.accent
                            selectedTextColor: "#ffffff"
                            selectByMouse: true
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            clip: true
                            onEditingFinished: root.settingsObj.onlineNeteaseApiUrl = text

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: "http://127.0.0.1:3000"
                                color: DesignTokens.textTertiary
                                font: neteaseUrlInput.font
                                visible: neteaseUrlInput.text.length === 0 && !neteaseUrlInput.inputMethodComposing
                            }
                        }
                    }
                }

                // Meting API URL (Optional)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Meting API URL (Optional Direct URL & Fallback)")
                            color: DesignTokens.textPrimary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        Text {
                            visible: root.onlineCtrl.metingTestStatus.length > 0
                            text: root.onlineCtrl.metingTestStatus
                            color: root.onlineCtrl.metingTestOk ? DesignTokens.badgeTeal : DesignTokens.textTertiary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        radius: 10
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: metingUrlInput.activeFocus ? DesignTokens.accent : DesignTokens.borderOuter

                        TextInput {
                            id: metingUrlInput
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: TextInput.AlignVCenter
                            text: root.settingsObj.onlineMetingApiUrl
                            color: DesignTokens.textPrimary
                            selectionColor: DesignTokens.accent
                            selectedTextColor: "#ffffff"
                            selectByMouse: true
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                            clip: true
                            onEditingFinished: root.settingsObj.onlineMetingApiUrl = text

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: "http://127.0.0.1:3001/api"
                                color: DesignTokens.textTertiary
                                font: metingUrlInput.font
                                visible: metingUrlInput.text.length === 0 && !metingUrlInput.inputMethodComposing
                            }
                        }
                    }
                }

                // Connection Test Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    VirelaButton {
                        primary: true
                        compact: true
                        iconName: "refresh"
                        enabled: !root.onlineCtrl.testingConnection
                        text: root.onlineCtrl.testingConnection ? qsTr("Testing...") : qsTr("Test Connection")
                        onClicked: {
                            root.settingsObj.onlineNeteaseApiUrl = neteaseUrlInput.text
                            root.settingsObj.onlineMetingApiUrl = metingUrlInput.text
                            root.onlineCtrl.testConnections()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Leave URLs empty to disable online sources. Only connect to self-hosted or user-configured APIs.")
                        color: DesignTokens.textTertiary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 11
                    }
                }

                // Auto-start Local Managed Backends with Virela
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Auto-start Local Backends with Virela")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: root.onlineCtrl.localServicesStatus
                                color: (root.onlineCtrl.neteaseServiceRunning || root.onlineCtrl.metingServiceRunning)
                                    ? DesignTokens.badgeTeal : DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        VirelaButton {
                            visible: root.settingsObj.onlineAutoStartLocalServices
                            compact: true
                            iconName: "refresh"
                            text: qsTr("Restart")
                            onClicked: root.onlineCtrl.restartLocalServices()
                        }
                        VirelaSwitch {
                            checked: root.settingsObj.onlineAutoStartLocalServices
                            onToggled: (state) => root.settingsObj.onlineAutoStartLocalServices = state
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.settingsObj.onlineAutoStartLocalServices
                        Text {
                            Layout.preferredWidth: 118
                            text: qsTr("Netease Service Dir")
                            color: DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            radius: 8
                            color: DesignTokens.surfaceInput
                            border.width: 1
                            border.color: DesignTokens.borderSubtle
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                text: root.settingsObj.onlineNeteaseServiceDir.length > 0
                                    ? root.settingsObj.onlineNeteaseServiceDir : qsTr("Not set")
                                elide: Text.ElideMiddle
                                color: root.settingsObj.onlineNeteaseServiceDir.length > 0
                                    ? DesignTokens.textSecondary : DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        VirelaButton {
                            compact: true
                            iconName: "folder"
                            text: qsTr("Choose")
                            onClicked: root.selectNeteaseServiceDirRequested()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.settingsObj.onlineAutoStartLocalServices
                        Text {
                            Layout.preferredWidth: 118
                            text: qsTr("Meting Service Dir")
                            color: DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            radius: 8
                            color: DesignTokens.surfaceInput
                            border.width: 1
                            border.color: DesignTokens.borderSubtle
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                text: root.settingsObj.onlineMetingServiceDir.length > 0
                                    ? root.settingsObj.onlineMetingServiceDir : qsTr("Not set")
                                elide: Text.ElideMiddle
                                color: root.settingsObj.onlineMetingServiceDir.length > 0
                                    ? DesignTokens.textSecondary : DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        VirelaButton {
                            compact: true
                            iconName: "folder"
                            text: qsTr("Choose")
                            onClicked: root.selectMetingServiceDirRequested()
                        }
                    }
                }

                // Direct URL Priority
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Direct Stream URL Priority")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
                        Layout.preferredWidth: 186
                        model: [qsTr("Netease API First"), qsTr("Meting API First")]
                        currentIndex: root.settingsObj.onlineDirectUrlPriority === "meting" ? 1 : 0
                        onActivated: (idx) => {
                            root.settingsObj.onlineDirectUrlPriority = idx === 1 ? "meting" : "netease"
                        }
                    }
                }

                // Default Audio Quality
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Default Streaming & Download Quality")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaComboBox {
                        Layout.preferredWidth: 186
                        model: [qsTr("Standard (128k)"), qsTr("ExHigh (320k)"), qsTr("Lossless (FLAC)"), qsTr("Hi-Res")]
                        currentIndex: {
                            const q = root.settingsObj.onlineAudioQuality
                            if (q === "standard") return 0
                            if (q === "exhigh") return 1
                            if (q === "hires") return 3
                            return 2
                        }
                        onActivated: (idx) => {
                            const quals = ["standard", "exhigh", "lossless", "hires"]
                            root.settingsObj.onlineAudioQuality = quals[idx]
                        }
                    }
                }

                // Stream Cache LRU Cap & Clear
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Stream Cache LRU Limit")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    FormatBadge {
                        compact: true
                        text: root.onlineCtrl.cacheUsageText
                    }
                    VirelaComboBox {
                        Layout.preferredWidth: 128
                        model: ["256 MB", "512 MB", "1 GB", "2 GB", "4 GB", "8 GB"]
                        currentIndex: {
                            const mb = root.settingsObj.onlineCacheMaxMb
                            if (mb <= 256) return 0
                            if (mb <= 512) return 1
                            if (mb <= 1024) return 2
                            if (mb <= 2048) return 3
                            if (mb <= 4096) return 4
                            return 5
                        }
                        onActivated: (idx) => {
                            const vals = [256, 512, 1024, 2048, 4096, 8192]
                            root.settingsObj.onlineCacheMaxMb = vals[idx]
                        }
                    }
                    VirelaButton {
                        compact: true
                        iconName: "trash"
                        text: qsTr("Clear")
                        onClicked: root.onlineCtrl.clearStreamCache()
                    }
                }

                // Local Download Directory
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Local Download Directory")
                                color: DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Downloads embed cover art & metadata, save .yrc/.lrc lyrics sidecars, and auto-index into your local library.")
                                color: DesignTokens.textTertiary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 11
                            }
                        }
                        VirelaButton {
                            compact: true
                            iconName: "folder"
                            text: qsTr("Choose Folder")
                            onClicked: root.selectDownloadDirRequested()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: 9
                        color: DesignTokens.surfaceInput
                        border.width: 1
                        border.color: DesignTokens.borderSubtle

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: Text.AlignVCenter
                            text: root.settingsObj.onlineDownloadDir
                            elide: Text.ElideMiddle
                            color: DesignTokens.textSecondary
                            font.family: DesignTokens.fontFamily
                            font.pixelSize: 12
                        }
                    }
                }
            }

            // 4. Music Library Folders
            SectionCard {
                visible: root.activeSettingsTab === "local"
                iconName: "folder"
                title: qsTr("Watched Music Folders")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    VirelaButton {
                        primary: true
                        compact: true
                        iconName: "plus"
                        text: qsTr("Add Folder")
                        onClicked: root.addFolderRequested()
                    }

                    VirelaButton {
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

            // 5. Desktop Integration
            SectionCard {
                visible: root.activeSettingsTab === "appearance"
                iconName: "mini"
                title: qsTr("Desktop Integration (Windows)")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Minimize to System Tray on Close")
                        color: DesignTokens.textSecondary
                        font.family: DesignTokens.fontFamily
                        font.pixelSize: 13
                    }
                    VirelaSwitch {
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
                    VirelaSwitch {
                        checked: root.settingsObj.globalHotkeysEnabled
                        onToggled: (state) => root.settingsObj.globalHotkeysEnabled = state
                    }
                }
            }
        }
    }
    }
}
