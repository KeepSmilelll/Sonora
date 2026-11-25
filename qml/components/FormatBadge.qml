import QtQuick
import Sonora.UI

Rectangle {
    id: root

    property string text: ""
    property bool compact: false

    visible: text.length > 0
    implicitHeight: compact ? 18 : 20
    implicitWidth: label.implicitWidth + (compact ? 10 : 14)
    radius: height / 2

    readonly property color badgeColor: {
        if (text.indexOf("DSD") === 0) return DesignTokens.badgePurple
        if (text === "Hi-Res" || text.indexOf("Exclusive") >= 0) return DesignTokens.badgeGold
        if (text === "Lossless" || text === "FLAC" || text === "ALAC") return DesignTokens.badgeTeal
        return DesignTokens.textSecondary
    }

    color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, DesignTokens.isDark ? 0.16 : 0.12)
    border.width: 1
    border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.38)

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.badgeColor
        font.family: DesignTokens.fontFamily
        font.pixelSize: root.compact ? 10 : 11
        font.weight: Font.DemiBold
        font.letterSpacing: 0.2
    }
}
