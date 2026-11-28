import QtQuick
import Virela.UI

// Sample the existing playback clock so the indicator shares the progress bar's redraws.
Item {
    id: root

    required property var controller
    property color barColor: DesignTokens.accent
    property real size: 14

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    readonly property bool windowExposed: root.Window.window !== null
        && root.Window.window.visible
        && root.Window.window.visibility !== Window.Minimized
    readonly property bool animActive: visible && windowExposed && controller.isPlaying
    readonly property real barW: Math.max(2, Math.round(size * 0.19))
    readonly property real gap: Math.max(1.5, (size - barW * 3) / 2.0)

    readonly property real _phase: animActive ? Math.floor(controller.positionMs / 66) * 0.42 : 0

    Rectangle {
        width: root.barW
        height: root.animActive
            ? root.size * (0.28 + 0.64 * (0.5 + 0.5 * Math.sin(root._phase)))
            : Math.round(root.size * 0.35)
        radius: width / 2
        x: 0
        anchors.bottom: parent.bottom
        color: root.barColor
    }

    Rectangle {
        width: root.barW
        height: root.animActive
            ? root.size * (0.30 + 0.70 * (0.5 + 0.5 * Math.sin(root._phase * 1.27 + 2.1)))
            : Math.round(root.size * 0.62)
        radius: width / 2
        x: root.barW + root.gap
        anchors.bottom: parent.bottom
        color: root.barColor
    }

    Rectangle {
        width: root.barW
        height: root.animActive
            ? root.size * (0.25 + 0.63 * (0.5 + 0.5 * Math.sin(root._phase * 0.83 + 4.2)))
            : Math.round(root.size * 0.42)
        radius: width / 2
        x: (root.barW + root.gap) * 2
        anchors.bottom: parent.bottom
        color: root.barColor
    }
}
