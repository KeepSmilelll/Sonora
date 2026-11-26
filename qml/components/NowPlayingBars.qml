import QtQuick
import Sonora.UI

// Lightweight 3-bar animated now-playing equalizer indicator (zero CPU when hidden or paused)
Item {
    id: root

    property bool playing: false
    property color barColor: DesignTokens.accent
    property real size: 14

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    readonly property bool animActive: visible && playing
    readonly property real barW: Math.max(2, Math.round(size * 0.19))
    readonly property real gap: Math.max(1.5, (size - barW * 3) / 2.0)

    Rectangle {
        id: b1
        width: root.barW
        height: root.animActive ? h1 : Math.round(root.size * 0.35)
        radius: width / 2
        x: 0
        anchors.bottom: parent.bottom
        color: root.barColor
        property real h1: root.size * 0.35

        Behavior on height { NumberAnimation { duration: 140 } }

        SequentialAnimation on h1 {
            running: root.animActive
            loops: Animation.Infinite
            NumberAnimation { to: root.size * 0.92; duration: 320; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 0.28; duration: 290; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 0.68; duration: 250; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        id: b2
        width: root.barW
        height: root.animActive ? h2 : Math.round(root.size * 0.62)
        radius: width / 2
        x: root.barW + root.gap
        anchors.bottom: parent.bottom
        color: root.barColor
        property real h2: root.size * 0.85

        Behavior on height { NumberAnimation { duration: 140 } }

        SequentialAnimation on h2 {
            running: root.animActive
            loops: Animation.Infinite
            NumberAnimation { to: root.size * 0.30; duration: 270; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 1.00; duration: 340; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 0.48; duration: 260; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        id: b3
        width: root.barW
        height: root.animActive ? h3 : Math.round(root.size * 0.42)
        radius: width / 2
        x: (root.barW + root.gap) * 2
        anchors.bottom: parent.bottom
        color: root.barColor
        property real h3: root.size * 0.50

        Behavior on height { NumberAnimation { duration: 140 } }

        SequentialAnimation on h3 {
            running: root.animActive
            loops: Animation.Infinite
            NumberAnimation { to: root.size * 0.86; duration: 260; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 0.25; duration: 330; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.size * 0.72; duration: 300; easing.type: Easing.InOutSine }
        }
    }
}
