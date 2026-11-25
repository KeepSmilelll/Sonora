pragma Singleton
import QtQuick

QtObject {
    id: root

    // Updated from root Main.qml whenever ThemeManager changes
    property bool isDark: true
    property bool nativeBackdropActive: false
    property string backdropMode: "mica"
    property color ambientPrimary: "#2a3048"
    property color ambientSecondary: "#181c2a"
    property color accent: "#fa4866"

    // Typography (Windows 10/11 Segoe UI Variable + Microsoft YaHei UI fallback)
    readonly property string fontFamily: Qt.platform.os === "windows"
        ? "Segoe UI Variable, Microsoft YaHei UI, Segoe UI"
        : "Inter, Noto Sans CJK SC, Sans-Serif"

    // Window & surface fills — translucent when Win11 Mica/Acrylic is active
    readonly property color windowBg: {
        if (nativeBackdropActive) {
            return isDark ? Qt.rgba(0.06, 0.065, 0.085, 0.42) : Qt.rgba(0.97, 0.975, 0.99, 0.48)
        }
        return isDark ? "#11131a" : "#f5f6fa"
    }

    readonly property color sidebarBg: isDark
        ? Qt.rgba(0.08, 0.085, 0.11, nativeBackdropActive ? 0.45 : 0.62)
        : Qt.rgba(1.0, 1.0, 1.0, nativeBackdropActive ? 0.52 : 0.70)

    readonly property color surfaceCard: isDark
        ? Qt.rgba(0.13, 0.14, 0.18, nativeBackdropActive ? 0.54 : 0.68)
        : Qt.rgba(1.0, 1.0, 1.0, nativeBackdropActive ? 0.68 : 0.82)

    readonly property color surfaceElevated: isDark
        ? Qt.rgba(0.16, 0.17, 0.22, 0.86)
        : Qt.rgba(1.0, 1.0, 1.0, 0.92)

    readonly property color surfaceHover: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.07)
        : Qt.rgba(0.0, 0.0, 0.0, 0.045)

    readonly property color surfacePressed: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.12)
        : Qt.rgba(0.0, 0.0, 0.0, 0.08)

    readonly property color surfaceSelected: Qt.rgba(accent.r, accent.g, accent.b, isDark ? 0.22 : 0.14)

    readonly property color playbackBarBg: isDark
        ? Qt.rgba(0.10, 0.11, 0.15, nativeBackdropActive ? 0.74 : 0.86)
        : Qt.rgba(0.99, 0.99, 1.0, nativeBackdropActive ? 0.78 : 0.90)

    // Crisp hairline borders
    readonly property color borderSubtle: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.08)
        : Qt.rgba(0.0, 0.0, 0.0, 0.07)

    readonly property color borderHighlight: isDark
        ? Qt.rgba(1.0, 1.0, 1.0, 0.14)
        : Qt.rgba(1.0, 1.0, 1.0, 0.85)

    // Text hierarchy
    readonly property color textPrimary: isDark ? "#f4f6fb" : "#161822"
    readonly property color textSecondary: isDark ? Qt.rgba(0.92, 0.94, 0.98, 0.68) : Qt.rgba(0.09, 0.10, 0.14, 0.64)
    readonly property color textTertiary: isDark ? Qt.rgba(0.92, 0.94, 0.98, 0.44) : Qt.rgba(0.09, 0.10, 0.14, 0.42)

    // Hi-Res / Lossless / DSD semantic accents
    readonly property color badgeGold: isDark ? "#f0b44c" : "#b8790f"
    readonly property color badgeTeal: isDark ? "#4cd6b5" : "#0e8a6d"
    readonly property color badgePurple: isDark ? "#b88cff" : "#6c3dd6"

    // Spacing & Radii
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 18
    readonly property int sidebarWidth: 224
    readonly property int titleBarHeight: 44
    readonly property int playbackBarHeight: 82
}
