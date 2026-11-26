import QtQuick
import Sonora.UI

// Multi-layered fluid aurora cover backdrop (zero offscreen FBO blur cost)
Item {
    id: root

    property bool immersive: false
    property bool playing: false

    readonly property bool showGlow: DesignTokens.backdropMode !== "solid"
    readonly property real glowStrength: {
        if (!showGlow) return 0.0
        if (immersive) return DesignTokens.isDark ? 0.94 : 0.88
        if (DesignTokens.nativeBackdropActive) return DesignTokens.isDark ? 0.28 : 0.20
        return DesignTokens.isDark ? 0.72 : 0.60
    }

    property color animatedPrimary: DesignTokens.ambientPrimary
    property color animatedSecondary: DesignTokens.ambientSecondary
    property color animatedAccent: DesignTokens.accent
    property real breathPhase: 1.0

    Behavior on animatedPrimary {
        ColorAnimation { duration: 750; easing.type: Easing.OutCubic }
    }
    Behavior on animatedSecondary {
        ColorAnimation { duration: 750; easing.type: Easing.OutCubic }
    }
    Behavior on animatedAccent {
        ColorAnimation { duration: 750; easing.type: Easing.OutCubic }
    }

    SequentialAnimation on breathPhase {
        running: root.visible && root.playing && root.glowStrength > 0.01
        loops: Animation.Infinite
        NumberAnimation { to: 0.84; duration: 3400; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1.0;  duration: 3400; easing.type: Easing.InOutSine }
    }

    Rectangle {
        anchors.fill: parent
        color: root.immersive
            ? (DesignTokens.isDark ? "#090b12" : "#f1f3fa")
            : DesignTokens.windowBg
        Behavior on color { ColorAnimation { duration: DesignTokens.animNormal } }
    }

    // Primary top-left aurora wash
    Rectangle {
        anchors.fill: parent
        visible: root.glowStrength > 0.01
        opacity: root.glowStrength * root.breathPhase
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0;  color: root.animatedPrimary }
            GradientStop { position: 0.52; color: root.animatedSecondary }
            GradientStop { position: 1.0;  color: "transparent" }
        }
    }

    // Secondary diagonal counter-bloom
    Rectangle {
        anchors.fill: parent
        visible: root.glowStrength > 0.01
        opacity: root.glowStrength * 0.52
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0;  color: Qt.rgba(root.animatedPrimary.r, root.animatedPrimary.g, root.animatedPrimary.b, 0.24) }
            GradientStop { position: 0.55; color: "transparent" }
            GradientStop { position: 1.0;  color: Qt.rgba(root.animatedAccent.r, root.animatedAccent.g, root.animatedAccent.b, DesignTokens.isDark ? 0.18 : 0.12) }
        }
    }

    // Soft ambient bottom horizon tint behind Floating Playback Dock
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.38
        visible: root.glowStrength > 0.01
        opacity: root.glowStrength * 0.36
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.rgba(root.animatedSecondary.r, root.animatedSecondary.g, root.animatedSecondary.b, 0.55) }
        }
    }
}
