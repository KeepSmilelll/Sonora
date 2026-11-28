import QtQuick
import Virela.UI

// Sculpted spinning circular vinyl LP disc with concentric micro-grooves, circular cover label,
// spindle hole, fixed diagonal specular sheen, and motor inertia via 30 Hz Timer when exposed.
Item {
    id: root

    property string coverUrl: ""
    property bool playing: false
    property bool compact: false
    property real discRotation: 0.0
    property bool ownSpin: false
    property bool hovered: false

    readonly property bool windowExposed: root.Window.window !== null
        && root.Window.window.visible
        && root.Window.window.visibility !== Window.Minimized

    property real _spinSpeed: 0.0
    property double _lastSpinMs: 0.0
    readonly property real _targetSpeed: root.playing ? 24.0 : 0.0

    Timer {
        interval: 33
        repeat: true
        running: root.ownSpin && root.visible && root.windowExposed && (root.playing || root._spinSpeed > 0.15)
        onRunningChanged: if (running) root._lastSpinMs = Date.now()
        onTriggered: {
            const now = Date.now()
            const dt = root._lastSpinMs > 0
                ? Math.max(0.005, Math.min(0.08, (now - root._lastSpinMs) / 1000.0))
                : 0.033
            root._lastSpinMs = now
            const accel = root.playing ? 4.8 : 2.8
            root._spinSpeed += (root._targetSpeed - root._spinSpeed) * Math.min(1.0, dt * accel)
            if (!root.playing && root._spinSpeed < 0.15) {
                root._spinSpeed = 0.0
                return
            }
            root.discRotation = (root.discRotation + root._spinSpeed * dt) % 360.0
        }
    }

    // Outer circular vinyl disc
    Rectangle {
        id: vinylBody
        anchors.fill: parent
        radius: width * 0.5
        gradient: Gradient {
            GradientStop { position: 0.0;  color: "#1b1d26" }
            GradientStop { position: 0.48; color: "#0b0c11" }
            GradientStop { position: 1.0;  color: "#171922" }
        }
        border.width: 1
        border.color: root.hovered ? DesignTokens.accent : DesignTokens.borderHighlight
        Behavior on border.color { ColorAnimation { duration: 150 } }

        // Concentric vinyl micro-grooves (full-size disc only)
        Repeater {
            model: root.compact ? 0 : 4
            delegate: Rectangle {
                required property int index
                readonly property real ratio: 0.93 - index * 0.068
                anchors.centerIn: parent
                width: Math.round(vinylBody.width * ratio)
                height: width
                radius: width * 0.5
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, index % 2 === 0 ? 0.055 : 0.032)
            }
        }

        // Rotating vinyl rotor (grooves markers + cover art center label + spindle collar)
        Item {
            id: rotor
            anchors.fill: parent
            rotation: root.discRotation

            // Subtle opposite groove markers so disc rotation stays visually legible even without artwork
            Rectangle {
                visible: !root.compact
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Math.max(4, Math.round(parent.width * 0.045))
                width: 2
                height: Math.max(6, Math.round(parent.width * 0.065))
                radius: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.16)
            }
            Rectangle {
                visible: !root.compact
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Math.max(4, Math.round(parent.width * 0.045))
                width: 2
                height: Math.max(6, Math.round(parent.width * 0.065))
                radius: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.16)
            }

            // Circular center record label
            Rectangle {
                id: labelBox
                anchors.centerIn: parent
                width: Math.round(parent.width * (root.compact ? 0.72 : 0.64))
                height: width
                radius: width * 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#2e3240" }
                    GradientStop { position: 0.5; color: "#1c1f29" }
                    GradientStop { position: 1.0; color: "#262a37" }
                }

                Rectangle {
                    id: labelMask
                    anchors.fill: parent
                    radius: labelBox.radius
                    visible: false
                    layer.enabled: coverImg.status === Image.Ready
                    layer.smooth: true
                }

                Image {
                    id: coverImg
                    anchors.fill: parent
                    source: root.coverUrl
                    sourceSize: Qt.size(root.compact ? 96 : 360, root.compact ? 96 : 360)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    visible: false
                }

                Loader {
                    anchors.fill: coverImg
                    active: coverImg.status === Image.Ready
                    onActiveChanged: {
                        if (active) {
                            setSource(Qt.resolvedUrl("VinylCoverMask.qml"), {
                                sourceItem: coverImg,
                                maskItem: labelMask
                            })
                        } else {
                            source = ""
                        }
                    }
                    Component.onCompleted: {
                        if (active) {
                            setSource(Qt.resolvedUrl("VinylCoverMask.qml"), {
                                sourceItem: coverImg,
                                maskItem: labelMask
                            })
                        }
                    }
                }

                Rectangle {
                    visible: coverImg.status !== Image.Ready
                    anchors.centerIn: parent
                    width: Math.round(labelBox.width * 0.84)
                    height: width
                    radius: width * 0.5
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, 0.16)
                }

                Rectangle {
                    visible: !root.compact && coverImg.status !== Image.Ready
                    anchors.centerIn: parent
                    width: Math.round(labelBox.width * 0.50)
                    height: width
                    radius: width * 0.5
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, 0.09)
                }

                Text {
                    visible: !root.compact && coverImg.status !== Image.Ready
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: Math.round(labelBox.height * 0.21)
                    text: "VIRELA"
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.42)
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: Math.max(8, Math.round(labelBox.width * 0.07))
                    font.letterSpacing: 2.0
                    font.weight: Font.DemiBold
                }

                Text {
                    visible: !root.compact && coverImg.status !== Image.Ready
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: Math.round(labelBox.height * 0.72)
                    text: "33⅓ RPM"
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.24)
                    font.family: DesignTokens.fontFamily
                    font.pixelSize: Math.max(7, Math.round(labelBox.width * 0.055))
                    font.letterSpacing: 1.0
                }

                // Crisp label edge rim
                Rectangle {
                    visible: true
                    anchors.fill: parent
                    radius: labelBox.radius
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, coverImg.status === Image.Ready ? 0.22 : 0.14)
                }
            }

            // Center spindle collar & hole
            Rectangle {
                id: spindleCollar
                readonly property int holeSize: root.compact ? 8 : Math.max(14, Math.round(vinylBody.width * 0.095))
                anchors.centerIn: parent
                width: holeSize + (root.compact ? 4 : 8)
                height: width
                radius: width * 0.5
                color: Qt.rgba(1.0, 1.0, 1.0, 0.14)
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.28)

                Rectangle {
                    anchors.centerIn: parent
                    width: spindleCollar.holeSize
                    height: width
                    radius: width * 0.5
                    color: DesignTokens.isDark ? "#0b0c10" : "#1c1f29"
                    border.width: 1
                    border.color: Qt.rgba(0.0, 0.0, 0.0, 0.45)
                }
            }
        }

        // Fixed diagonal specular sheen (circular so rotating by -35deg preserves exact round silhouette)
        Rectangle {
            anchors.fill: parent
            radius: width * 0.5
            rotation: -35
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0;  color: "transparent" }
                GradientStop { position: 0.38; color: "transparent" }
                GradientStop { position: 0.50; color: Qt.rgba(1.0, 1.0, 1.0, root.compact ? 0.06 : 0.085) }
                GradientStop { position: 0.62; color: "transparent" }
                GradientStop { position: 1.0;  color: "transparent" }
            }
        }
    }
}
