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

    Rectangle {
        anchors.fill: parent
        color: DesignTokens.solidWindowBg
        Behavior on color { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
    }

    FluidAuroraItem {
        anchors.fill: parent
        primaryColor: DesignTokens.ambientPrimary
        secondaryColor: DesignTokens.ambientSecondary
        accentColor: DesignTokens.accent
        baseColor: DesignTokens.solidWindowBg
        darkMode: DesignTokens.isDark
        active: root.visible && (!root.isSolid || intensity > 0.0)
        playing: root.playing
        intensity: root.isSolid ? 0.0 : (0.72 + 0.24 * root.immersiveFactor)
        immersiveFactor: root.immersiveFactor
        audioEnergy: root.audioEnergy

        Behavior on baseColor { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
        Behavior on intensity { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
    }
}
