pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property bool isDark: themeManager.isDark
    readonly property bool nativeBackdropActive: false
    readonly property string backdropMode: themeManager.backdropMode
    readonly property color ambientPrimary: themeManager.ambientPrimary
    readonly property color ambientSecondary: themeManager.ambientSecondary
    readonly property color accent: themeManager.accentColor

    readonly property string fontFamily: {
        const families = Qt.fontFamilies()
        if (families.includes("Noto Sans SC")) return "Noto Sans SC"
        if (families.includes("Noto Sans CJK SC")) return "Noto Sans CJK SC"
        return Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "sans-serif"
    }

    // Base Window & Spatial Glass Layering
    readonly property color solidWindowBg: isDark ? "#0b0d14" : "#f1f3f9"
    readonly property color windowBg: solidWindowBg

    readonly property color canvasBg: isDark
        ? Qt.rgba(0.085, 0.092, 0.128, 0.64)
        : Qt.rgba(1.0, 1.0, 1.0, 0.72)

    readonly property color sidebarBg: isDark
        ? Qt.rgba(0.095, 0.102, 0.142, 0.72)
        : Qt.rgba(1.0, 1.0, 1.0, 0.78)

    readonly property color sidebarGlass: sidebarBg

    readonly property color dockGlass: isDark
        ? Qt.rgba(0.115, 0.124, 0.172, 0.90)
        : Qt.rgba(0.99, 0.992, 1.0, 0.92)

    readonly property color playbackBarBg: dockGlass

    readonly property color surfaceCard: isDark
        ? Qt.rgba(0.14, 0.15, 0.20, 0.68)
        : Qt.rgba(1.0, 1.0, 1.0, 0.84)

    readonly property color surfaceCardHover: isDark
        ? Qt.rgba(0.18, 0.195, 0.255, 0.78)
        : Qt.rgba(1.0, 1.0, 1.0, 0.94)

    readonly property color surfaceElevated: isDark
        ? Qt.rgba(0.135, 0.145, 0.195, 0.94)
        : Qt.rgba(0.99, 0.992, 1.0, 0.96)

    readonly property color surfaceInput: isDark
        ? Qt.rgba(0.06, 0.065, 0.095, 0.58)
        : Qt.rgba(0.94, 0.95, 0.98, 0.78)

    readonly property color surfaceHover: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.075)
        : Qt.rgba(0.05, 0.07, 0.14, 0.05)

    readonly property color surfacePressed: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.13)
        : Qt.rgba(0.05, 0.07, 0.14, 0.09)

    readonly property color surfaceSelected: Qt.rgba(accent.r, accent.g, accent.b, isDark ? 0.20 : 0.13)
    readonly property color surfaceScrim: isDark ? Qt.rgba(0.02, 0.025, 0.05, 0.56) : Qt.rgba(0.06, 0.08, 0.14, 0.34)

    // Accent tonal helpers
    readonly property color accentLight: Qt.lighter(accent, isDark ? 1.18 : 1.10)
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, isDark ? 0.16 : 0.11)
    readonly property color accentGlow: Qt.rgba(accent.r, accent.g, accent.b, isDark ? 0.34 : 0.24)

    // Dual-rim visionOS / Apple Music borders
    readonly property color borderSubtle: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.075)
        : Qt.rgba(0.06, 0.08, 0.16, 0.075)

    readonly property color borderOuter: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.10)
        : Qt.rgba(0.06, 0.08, 0.16, 0.085)

    readonly property color borderHighlight: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.18)
        : Qt.rgba(0.06, 0.08, 0.16, 0.16)

    readonly property color borderHover: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.26)
        : Qt.rgba(0.06, 0.08, 0.16, 0.24)

    readonly property color borderSpecularTop: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.16)
        : Qt.rgba(1.0, 1.0, 1.0, 0.55)

    // Virela 2.0 L0 / L1 / L2 Depth & Borderless Spatial Surfaces
    readonly property color hairlineBorder: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.055)
        : Qt.rgba(0.06, 0.08, 0.16, 0.06)

    readonly property color railSurfaceBg: isDark
        ? Qt.rgba(0.082, 0.088, 0.125, 0.68)
        : Qt.rgba(1.0, 1.0, 1.0, 0.78)

    readonly property color stageSurfaceBg: isDark
        ? Qt.rgba(0.090, 0.098, 0.140, 0.66)
        : Qt.rgba(1.0, 1.0, 1.0, 0.80)

    readonly property color contentStageBg: isDark
        ? Qt.rgba(0.078, 0.085, 0.120, 0.54)
        : Qt.rgba(1.0, 1.0, 1.0, 0.68)

    readonly property color dockFloatingBg: isDark
        ? Qt.rgba(0.11, 0.12, 0.17, 0.88)
        : Qt.rgba(0.99, 0.995, 1.0, 0.92)

    readonly property color dockImmersiveBg: isDark
        ? Qt.rgba(0.10, 0.11, 0.16, 0.58)
        : Qt.rgba(1.0, 1.0, 1.0, 0.68)

    // Text hierarchy
    readonly property color textPrimary: isDark ? "#f5f7fc" : "#141722"
    readonly property color textSecondary: isDark ? "#b8bdcc" : Qt.rgba(0.08, 0.10, 0.16, 0.66)
    readonly property color textTertiary: isDark ? "#8d93a7" : Qt.rgba(0.08, 0.10, 0.16, 0.44)

    // Semantic audio badges
    readonly property color badgeGold: isDark ? "#f4ba54" : "#b07108"
    readonly property color badgeTeal: isDark ? "#45d9b8" : "#0b8267"
    readonly property color badgePurple: isDark ? "#be92ff" : "#6b3bd4"

    readonly property color playbackSurface: isDark ? "#1c1e2c" : dockGlass
    readonly property color playbackInk: isDark ? "#190711" : "#ffffff"
    readonly property color playbackRail: isDark ? "#383c4d" : "#c9cdd8"
    readonly property color librarySurface: isDark ? "#171a25" : surfaceCard
    readonly property color queueSurface: isDark ? "#212433" : surfaceElevated

    // Spatial Radii & Metrics
    readonly property int radiusSm: 8
    readonly property int radiusMd: 12
    readonly property int radiusLg: 16
    readonly property int radiusXl: 22
    readonly property int radiusDock: 22
    readonly property int radiusPill: 999

    readonly property int sidebarWidth: 228
    readonly property int compactSidebarWidth: 144
    readonly property int railCollapsedWidth: 60
    readonly property int railCompactExpandedWidth: 196
    readonly property int railExpandedWidth: 212
    readonly property int railWideExpandedWidth: 232
    readonly property int stageMinWidth: 276
    readonly property int stageMaxWidth: 520
    readonly property int dockMaxWidth: 1540
    readonly property int dockRailMaxWidth: 960
    readonly property int dockImmersiveMaxWidth: 780
    readonly property int titleBarHeight: 46
    readonly property int dockHeight: 76
    readonly property int dockShortHeight: 68
    readonly property int dockMarginX: 16
    readonly property int dockMarginBottom: 12
    readonly property int dockClearance: dockHeight + dockMarginBottom + 10
    readonly property int playbackBarHeight: dockHeight
    readonly property int settingsMaxWidth: 960

    function computeRailCoverGeometry(stageW, stageH) {
        const maxByH = (stageH && stageH > 0) ? Math.max(96, Math.round(stageH * 0.29)) : 196
        const maxByW = Math.max(96, Math.round(stageW - 76))
        const cap = (stageW >= 380 && (!stageH || stageH >= 740)) ? 216 : 196
        const art = Math.round(Math.max(96, Math.min(cap, maxByW, maxByH)))
        return { x: Math.round((stageW - art) * 0.5), y: 64, width: art, height: art, radius: art * 0.5 }
    }

    function computeImmersiveCoverGeometry(stageW, stageH, vizEnabled) {
        const bodyW = Math.max(300, stageW - 32)
        const bodyH = Math.max(240, stageH - 72)
        const contentW = Math.min(bodyW, 1520)
        const contentOffsetX = Math.round((bodyW - contentW) * 0.5)
        const leftColW = Math.round(Math.max(296, Math.min(520, contentW * 0.40)))
        const controlsH = 66 + 36 + (vizEnabled ? 44 : 0) + 46 + 52 + 42
        const maxArt = stageW >= 1800 ? 376 : (stageW >= 1440 ? 344 : 308)
        const art = Math.round(Math.max(152, Math.min(maxArt, leftColW - 56, bodyH - controlsH - 20)))
        const groupH = art + controlsH
        const groupY = Math.max(6, Math.round((bodyH - groupH) * 0.45))
        return {
            contentW: contentW,
            contentOffsetX: contentOffsetX,
            leftColW: leftColW,
            groupH: groupH,
            groupY: groupY,
            x: 16 + contentOffsetX + Math.round((leftColW - art) * 0.5),
            y: 56 + groupY,
            width: art,
            height: art,
            radius: art * 0.5
        }
    }

    // Motion Durations (ms)
    readonly property int animFast: 150
    readonly property int animNormal: 240
    readonly property int animPanel: 300
    readonly property int animSheet: 360
    readonly property int animStage: 380
    readonly property int animHero: 420
}
