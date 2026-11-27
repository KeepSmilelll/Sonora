import QtQuick
import Virela.UI

// GPU procedural fluid aurora backdrop powered by single-draw-call FluidAuroraItem
Item {
    id: root

    property bool immersive: false
    property real immersiveFactor: immersive ? 1.0 : 0.0
    property bool playing: false
    property real audioEnergy: 0.0

    readonly property bool isSolid: DesignTokens.backdropMode === "solid"
    readonly property real auroraIntensity: {
        if (isSolid) return 0.0
        if (DesignTokens.nativeBackdropActive) {
            return 0.34 + 0.58 * root.immersiveFactor
        }
        return 0.72 + 0.24 * root.immersiveFactor
    }

    Rectangle {
        anchors.fill: parent
        color: {
            if (DesignTokens.nativeBackdropActive) {
                const bg = DesignTokens.solidWindowBg
                const alpha = 0.08 + 0.78 * root.immersiveFactor
                return Qt.rgba(bg.r, bg.g, bg.b, alpha)
            }
            return DesignTokens.solidWindowBg
        }
        Behavior on color { ColorAnimation { duration: DesignTokens.animNormal } }
    }

    FluidAuroraItem {
        anchors.fill: parent
        primaryColor: DesignTokens.ambientPrimary
        secondaryColor: DesignTokens.ambientSecondary
        accentColor: DesignTokens.accent
        baseColor: DesignTokens.nativeBackdropActive
            ? Qt.rgba(DesignTokens.solidWindowBg.r, DesignTokens.solidWindowBg.g, DesignTokens.solidWindowBg.b,
                      0.12 + 0.76 * root.immersiveFactor)
            : DesignTokens.solidWindowBg
        darkMode: DesignTokens.isDark
        active: root.visible && !root.isSolid
        playing: root.playing
        intensity: root.auroraIntensity
        immersiveFactor: root.immersiveFactor
        audioEnergy: root.audioEnergy
    }
}
