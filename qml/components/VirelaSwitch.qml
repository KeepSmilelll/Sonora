import QtQuick
import Virela.UI

// Apple / visionOS tactile capsule toggle switch with spring thumb & press stretch
Item {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 44
    implicitHeight: 24
    opacity: enabled ? 1.0 : 0.42

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked
            ? DesignTokens.accent
            : (ma.containsMouse ? DesignTokens.surfacePressed : DesignTokens.surfaceInput)
        border.width: 1
        border.color: root.checked
            ? Qt.lighter(DesignTokens.accent, 1.15)
            : DesignTokens.borderOuter

        Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        Rectangle {
            id: thumb
            readonly property real restW: 18
            readonly property real pressW: 22
            width: ma.pressed ? pressW : restW
            height: 18
            radius: 9
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? (track.width - width - 3) : 3
            color: "#ffffff"
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.12)

            Behavior on x {
                NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 1.08 }
            }
            Behavior on width {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
