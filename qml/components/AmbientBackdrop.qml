import QtQuick
import Sonora.UI

// Ultra-lightweight ambient cover-color backdrop (zero offscreen FBO blur buffers)
Item {
    id: root

    property bool immersive: false

    readonly property bool showGlow: DesignTokens.backdropMode !== "solid"
    readonly property real glowStrength: {
        if (!showGlow) return 0.0
        if (immersive) return DesignTokens.isDark ? 0.92 : 0.85
        if (DesignTokens.nativeBackdropActive) return DesignTokens.isDark ? 0.22 : 0.16
        return DesignTokens.isDark ? 0.65 : 0.55
    }

    property color animatedPrimary: DesignTokens.ambientPrimary
    property color animatedSecondary: DesignTokens.ambientSecondary

    Behavior on animatedPrimary {
        ColorAnimation { duration: 550; easing.type: Easing.OutCubic }
    }
    Behavior on animatedSecondary {
        ColorAnimation { duration: 550; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        color: root.immersive
            ? (DesignTokens.isDark ? "#0d0f16" : "#f4f5fa")
            : DesignTokens.windowBg
    }

    // Primary top-left ambient wash
    Rectangle {
        anchors.fill: parent
        visible: root.glowStrength > 0.01
        opacity: root.glowStrength
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: root.animatedPrimary }
            GradientStop { position: 0.55; color: root.animatedSecondary }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Subtle secondary diagonal highlight wash
    Rectangle {
        anchors.fill: parent
        visible: root.glowStrength > 0.01
        opacity: root.glowStrength * 0.45
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.6; color: Qt.rgba(root.animatedPrimary.r, root.animatedPrimary.g, root.animatedPrimary.b, 0.35) }
            GradientStop { position: 1.0; color: Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, DesignTokens.isDark ? 0.14 : 0.10) }
        }
    }
}
