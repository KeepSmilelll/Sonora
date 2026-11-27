import QtQuick
import Virela.UI

// Spatial glass slider supporting Horizontal & Vertical orientations + Bipolar center-origin EQ fill
Item {
    id: root

    property real from: 0.0
    property real to: 1.0
    property real value: 0.0
    property real stepSize: 0.0
    property int orientation: Qt.Horizontal
    property bool bipolar: false
    property color accentColor: DesignTokens.accent
    signal moved(real newValue)

    readonly property bool isVertical: orientation === Qt.Vertical
    readonly property real range: Math.max(1e-6, to - from)
    readonly property real normalized: Math.max(0.0, Math.min(1.0, (value - from) / range))

    implicitWidth: isVertical ? 26 : 120
    implicitHeight: isVertical ? 140 : 24
    opacity: enabled ? 1.0 : 0.38

    function snapValue(rawVal) {
        let clamped = Math.max(root.from, Math.min(root.to, rawVal))
        if (root.stepSize > 0.0) {
            clamped = Math.round((clamped - root.from) / root.stepSize) * root.stepSize + root.from
            clamped = Math.max(root.from, Math.min(root.to, clamped))
        }
        return clamped
    }

    function updateFromMouse(mx, my) {
        const pad = 7
        let ratio = 0.0
        if (root.isVertical) {
            const span = Math.max(1, height - pad * 2)
            ratio = 1.0 - Math.max(0.0, Math.min(1.0, (my - pad) / span))
        } else {
            const span = Math.max(1, width - pad * 2)
            ratio = Math.max(0.0, Math.min(1.0, (mx - pad) / span))
        }
        const nextVal = snapValue(root.from + ratio * root.range)
        if (Math.abs(nextVal - root.value) > 1e-5) {
            root.value = nextVal
            root.moved(nextVal)
        }
    }

    // Track groove
    Rectangle {
        id: groove
        property real thickness: (ma.containsMouse || ma.pressed) ? 6 : 4
        Behavior on thickness { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

        x: root.isVertical ? (root.width - thickness) / 2 : 7
        y: root.isVertical ? 7 : (root.height - thickness) / 2
        width: root.isVertical ? thickness : Math.max(4, root.width - 14)
        height: root.isVertical ? Math.max(4, root.height - 14) : thickness
        radius: thickness / 2
        color: DesignTokens.isDark ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0.06, 0.08, 0.16, 0.11)

        // Zero-center tick mark for bipolar EQ sliders
        Rectangle {
            visible: root.bipolar
            x: root.isVertical ? -3 : (parent.width * 0.5 - 0.5)
            y: root.isVertical ? (parent.height * 0.5 - 0.5) : -3
            width: root.isVertical ? (parent.width + 6) : 1
            height: root.isVertical ? 1 : (parent.height + 6)
            color: DesignTokens.borderHighlight
        }

        // Active fill bar
        Rectangle {
            radius: groove.radius
            color: root.accentColor

            x: {
                if (root.isVertical) return 0
                if (root.bipolar) return Math.min(0.5, root.normalized) * groove.width
                return 0
            }
            y: {
                if (!root.isVertical) return 0
                if (root.bipolar) return (1.0 - Math.max(0.5, root.normalized)) * groove.height
                return (1.0 - root.normalized) * groove.height
            }
            width: {
                if (root.isVertical) return groove.width
                if (root.bipolar) return Math.max(2, Math.abs(root.normalized - 0.5) * groove.width)
                return groove.width * root.normalized
            }
            height: {
                if (!root.isVertical) return groove.height
                if (root.bipolar) return Math.max(2, Math.abs(root.normalized - 0.5) * groove.height)
                return groove.height * root.normalized
            }
        }
    }

    // Tactile circular thumb
    Rectangle {
        id: thumb
        width: 14
        height: 14
        radius: 7
        color: "#ffffff"
        border.width: 2
        border.color: root.accentColor

        x: root.isVertical
            ? (root.width - width) / 2
            : (7 + groove.width * root.normalized - width / 2)
        y: root.isVertical
            ? (7 + groove.height * (1.0 - root.normalized) - height / 2)
            : (root.height - height) / 2

        scale: ma.pressed ? 1.22 : (ma.containsMouse ? 1.08 : 0.92)
        Behavior on scale {
            NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.15 }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPressed: (mouse) => root.updateFromMouse(mouse.x, mouse.y)
        onPositionChanged: (mouse) => {
            if (pressed) root.updateFromMouse(mouse.x, mouse.y)
        }
        onWheel: (wheel) => {
            const step = root.stepSize > 0.0 ? root.stepSize : (root.range / 24.0)
            const dir = wheel.angleDelta.y > 0 ? 1.0 : -1.0
            const nextVal = root.snapValue(root.value + dir * step)
            if (Math.abs(nextVal - root.value) > 1e-5) {
                root.value = nextVal
                root.moved(nextVal)
            }
        }
    }
}
